/* SPDX-License-Identifier: MIT */
#include "cpp/cpp.h"

#include "arena.h"
#include "diag.h"

#include <ctype.h>
#include <string.h>

typedef struct {
    unsigned char ch;
    SourceLoc loc;
} LogicalChar;

struct Cpp {
    const SourceFile *source;
    LogicalChar *chars;
    size_t len;
    size_t pos;
    int starts_line;
    int pending_space;
    int in_comment;
    SourceLoc comment_loc;
};

static int trigraph(int ch)
{
    switch (ch) {
    case '=': return '#';
    case '/': return '\\';
    case '\'': return '^';
    case '(': return '[';
    case ')': return ']';
    case '!': return '|';
    case '<': return '{';
    case '>': return '}';
    case '-': return '~';
    default: return 0;
    }
}

static void append_logical(LogicalChar *out, size_t *len, int ch,
                           const SourceFile *source, int line, int col)
{
    out[*len].ch = (unsigned char)ch;
    out[*len].loc = (SourceLoc){ source, line, col };
    (*len)++;
}

Cpp *cpp_create(const SourceFile *source)
{
    const unsigned char *bytes = source_bytes(source);
    size_t size = source_size(source);
    LogicalChar *phase1 = arena_alloc((size + 1) * sizeof(*phase1));
    LogicalChar *phase2 = arena_alloc((size + 1) * sizeof(*phase2));
    size_t n1 = 0;
    size_t n2 = 0;
    int line = 1;
    int col = 1;
    Cpp *cpp;

    for (size_t i = 0; i < size;) {
        int ch = bytes[i];
        int replacement;

        if (ch == '?' && i + 2 < size && bytes[i + 1] == '?' &&
            (replacement = trigraph(bytes[i + 2])) != 0) {
            append_logical(phase1, &n1, replacement, source, line, col);
            i += 3;
            col += 3;
            continue;
        }
        if (ch == '\r') {
            append_logical(phase1, &n1, '\n', source, line, col);
            if (i + 1 < size && bytes[i + 1] == '\n')
                i += 2;
            else
                i++;
            line++;
            col = 1;
            continue;
        }
        append_logical(phase1, &n1, ch, source, line, col);
        i++;
        if (ch == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }

    for (size_t i = 0; i < n1; i++) {
        if (phase1[i].ch == '\\' && i + 1 < n1 && phase1[i + 1].ch == '\n') {
            i++;
            continue;
        }
        phase2[n2++] = phase1[i];
    }

    cpp = arena_alloc_zeroed(sizeof(*cpp));
    cpp->source = source;
    cpp->chars = phase2;
    cpp->len = n2;
    cpp->starts_line = 1;
    return cpp;
}

static int peek(const Cpp *cpp, size_t ahead)
{
    size_t at = cpp->pos + ahead;
    return at < cpp->len ? cpp->chars[at].ch : EOF;
}

static int is_ident_start(int ch)
{
    return ch == '_' || (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z');
}

static int is_ident_continue(int ch)
{
    return is_ident_start(ch) || (ch >= '0' && ch <= '9');
}

static CppToken make_token(Cpp *cpp, CppTokenKind kind, size_t start,
                           size_t end, int starts_line, int leading_space)
{
    size_t len = end - start;
    char *text = arena_alloc(len + 1);
    CppToken token;

    for (size_t i = 0; i < len; i++)
        text[i] = (char)cpp->chars[start + i].ch;
    text[len] = '\0';
    token.kind = kind;
    token.text = text;
    token.len = len;
    token.loc = start < cpp->len
              ? cpp->chars[start].loc
              : (SourceLoc){ cpp->source, 1, 1 };
    token.leading_space = leading_space;
    token.starts_line = starts_line;
    return token;
}

static CppToken newline_token(Cpp *cpp, size_t at)
{
    CppToken token = make_token(cpp, CPP_NEWLINE, at, at + 1,
                                cpp->starts_line, cpp->pending_space);
    cpp->pos = at + 1;
    cpp->starts_line = 1;
    cpp->pending_space = 0;
    return token;
}

CppToken cpp_next(Cpp *cpp)
{
    for (;;) {
        int ch;
        size_t start;
        int starts_line;
        int leading_space;

        if (cpp->in_comment) {
            while (cpp->pos < cpp->len) {
                if (peek(cpp, 0) == '*' && peek(cpp, 1) == '/') {
                    cpp->pos += 2;
                    cpp->in_comment = 0;
                    cpp->pending_space = 1;
                    break;
                }
                if (peek(cpp, 0) == '\n')
                    return newline_token(cpp, cpp->pos);
                cpp->pos++;
            }
            if (cpp->in_comment) {
                diag_error_at(cpp->comment_loc, "unterminated comment");
                cpp->in_comment = 0;
            }
            continue;
        }

        ch = peek(cpp, 0);
        if (ch == EOF)
            return (CppToken){ .kind = CPP_EOF,
                .loc = { cpp->source, cpp->len ? cpp->chars[cpp->len - 1].loc.line : 1,
                         cpp->len ? cpp->chars[cpp->len - 1].loc.col + 1 : 1 },
                .starts_line = cpp->starts_line,
                .leading_space = cpp->pending_space };
        if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f') {
            cpp->pending_space = 1;
            cpp->pos++;
            continue;
        }
        if (ch == '\n')
            return newline_token(cpp, cpp->pos);
        if (ch == '/' && peek(cpp, 1) == '*') {
            cpp->comment_loc = cpp->chars[cpp->pos].loc;
            cpp->pos += 2;
            cpp->in_comment = 1;
            cpp->pending_space = 1;
            continue;
        }
        if (ch == '/' && peek(cpp, 1) == '/') {
            cpp->pending_space = 1;
            cpp->pos += 2;
            while (cpp->pos < cpp->len && peek(cpp, 0) != '\n')
                cpp->pos++;
            continue;
        }

        start = cpp->pos;
        starts_line = cpp->starts_line;
        leading_space = cpp->pending_space;
        cpp->starts_line = 0;
        cpp->pending_space = 0;

        if (is_ident_start(ch)) {
            if (ch == 'L' && (peek(cpp, 1) == '\'' || peek(cpp, 1) == '"')) {
                ch = peek(cpp, 1);
                cpp->pos += 2;
                while (cpp->pos < cpp->len && peek(cpp, 0) != '\n') {
                    int c = peek(cpp, 0);
                    cpp->pos++;
                    if (c == '\\' && cpp->pos < cpp->len)
                        cpp->pos++;
                    else if (c == ch)
                        break;
                }
                return make_token(cpp, ch == '\'' ? CPP_CHAR : CPP_STRING,
                                  start, cpp->pos, starts_line, leading_space);
            }
            cpp->pos++;
            while (is_ident_continue(peek(cpp, 0)))
                cpp->pos++;
            return make_token(cpp, CPP_IDENT, start, cpp->pos,
                              starts_line, leading_space);
        }
        if ((ch >= '0' && ch <= '9') ||
            (ch == '.' && peek(cpp, 1) >= '0' && peek(cpp, 1) <= '9')) {
            int previous = 0;
            cpp->pos++;
            while (cpp->pos < cpp->len) {
                int c = peek(cpp, 0);
                if (is_ident_continue(c) || c == '.') {
                    previous = c;
                    cpp->pos++;
                    continue;
                }
                if ((c == '+' || c == '-') &&
                    (previous == 'e' || previous == 'E' ||
                     previous == 'p' || previous == 'P')) {
                    previous = c;
                    cpp->pos++;
                    continue;
                }
                break;
            }
            return make_token(cpp, CPP_NUMBER, start, cpp->pos,
                              starts_line, leading_space);
        }
        if (ch == '\'' || ch == '"') {
            cpp->pos++;
            while (cpp->pos < cpp->len && peek(cpp, 0) != '\n') {
                int c = peek(cpp, 0);
                cpp->pos++;
                if (c == '\\' && cpp->pos < cpp->len)
                    cpp->pos++;
                else if (c == ch)
                    break;
            }
            return make_token(cpp, ch == '\'' ? CPP_CHAR : CPP_STRING,
                              start, cpp->pos, starts_line, leading_space);
        }

        cpp->pos++;
        if ((ch == '.' && peek(cpp, 0) == '.' && peek(cpp, 1) == '.') ||
            ((ch == '<' || ch == '>') && peek(cpp, 0) == ch &&
             peek(cpp, 1) == '=') ||
            ((ch == '-' && peek(cpp, 0) == '>') ||
             (ch == '+' && peek(cpp, 0) == '+') ||
             (ch == '-' && peek(cpp, 0) == '-') ||
             (ch == '<' && (peek(cpp, 0) == '<' || peek(cpp, 0) == '=')) ||
             (ch == '>' && (peek(cpp, 0) == '>' || peek(cpp, 0) == '=')) ||
             (strchr("=!&|+*/%^#", ch) && peek(cpp, 0) == '=') ||
             (ch == '&' && peek(cpp, 0) == '&') ||
             (ch == '|' && peek(cpp, 0) == '|') ||
             (ch == '#' && peek(cpp, 0) == '#'))) {
            cpp->pos++;
            if ((ch == '.' && cpp->pos < cpp->len && peek(cpp, 0) == '.') ||
                ((ch == '<' || ch == '>') && cpp->pos < cpp->len &&
                 peek(cpp, 0) == '='))
                cpp->pos++;
        }
        return make_token(cpp, CPP_PUNCT, start, cpp->pos,
                          starts_line, leading_space);
    }
}
