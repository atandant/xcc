/* SPDX-License-Identifier: MIT */
#include "cpp/cpp.h"

#include "cpp/expr.h"
#include "cpp/macro.h"

#include "arena.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char ch;
    SourceLoc loc;
} LogicalChar;

typedef struct Expansion {
    CppToken *tokens;
    size_t len;
    size_t pos;
    struct Expansion *next;
} Expansion;

typedef struct Replay {
    CppStreamToken token;
    struct Replay *next;
} Replay;

typedef struct Conditional {
    int parent_active;
    int branch_active;
    int branch_taken;
    int saw_else;
    SourceLoc opening_loc;
    struct Conditional *next;
} Conditional;

struct Cpp {
    const SourceFile *source;
    LogicalChar *chars;
    size_t len;
    size_t pos;
    int starts_line;
    int pending_space;
    int in_comment;
    SourceLoc comment_loc;
    CppMacros *macros;
    Expansion *expansions;
    Replay *replay;
    Conditional *conditionals;
    int reported_unterminated;
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
    cpp->macros = cpp_macros_create();
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
    CppToken token = {0};

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

static CppToken scan_next(Cpp *cpp)
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

        size_t punct_len = 1;
        size_t punctuator_count;
        const char *const *punctuators = cpp_punctuators(&punctuator_count);
        for (size_t i = 0; i < punctuator_count; i++) {
            size_t len = strlen(punctuators[i]);
            size_t j;

            for (j = 0; j < len; j++)
                if (peek(cpp, j) != (unsigned char)punctuators[i][j])
                    break;
            if (j == len) {
                punct_len = len;
                break;
            }
        }
        cpp->pos += punct_len;
        return make_token(cpp, CPP_PUNCT, start, cpp->pos,
                          starts_line, leading_space);
    }
}

static int token_is(const CppToken *token, const char *text)
{
    return token->text && strcmp(token->text, text) == 0;
}

static void consume_directive_line(Cpp *cpp)
{
    CppToken token;

    do {
        token = scan_next(cpp);
    } while (token.kind != CPP_NEWLINE && token.kind != CPP_EOF);
}

static void append_token(CppToken **tokens, size_t *len, size_t *cap,
                         CppToken token)
{
    if (*len == *cap) {
        size_t new_cap = *cap ? *cap * 2 : 8;
        CppToken *new_tokens = realloc(*tokens,
                                       new_cap * sizeof(*new_tokens));
        if (!new_tokens)
            diag_fatal("out of memory reading preprocessing directive");
        *tokens = new_tokens;
        *cap = new_cap;
    }
    (*tokens)[(*len)++] = token;
}

static void define_macro(Cpp *cpp, SourceLoc directive_loc)
{
    CppToken name = scan_next(cpp);
    CppToken token;
    CppToken *parameters = NULL;
    CppToken *replacement = NULL;
    size_t parameter_count = 0;
    size_t parameter_cap = 0;
    size_t replacement_len = 0;
    size_t replacement_cap = 0;
    int function_like = 0;

    if (name.kind != CPP_IDENT) {
        diag_error_at(name.kind == CPP_EOF ? directive_loc : name.loc,
                      "macro name must be an identifier");
        if (name.kind != CPP_NEWLINE && name.kind != CPP_EOF)
            consume_directive_line(cpp);
        return;
    }

    token = scan_next(cpp);
    if (token.kind == CPP_PUNCT && token_is(&token, "(") &&
        !token.leading_space) {
        function_like = 1;
        token = scan_next(cpp);
        if (!token_is(&token, ")")) {
            for (;;) {
                if (token.kind != CPP_IDENT) {
                    diag_error_at(token.loc,
                                  "macro parameter must be an identifier");
                    if (token.kind != CPP_NEWLINE && token.kind != CPP_EOF)
                        consume_directive_line(cpp);
                    free(parameters);
                    return;
                }
                append_token(&parameters, &parameter_count, &parameter_cap,
                             token);
                token = scan_next(cpp);
                if (token_is(&token, ")"))
                    break;
                if (!token_is(&token, ",")) {
                    diag_error_at(token.loc,
                                  "expected ',' or ')' in macro parameter list");
                    if (token.kind != CPP_NEWLINE && token.kind != CPP_EOF)
                        consume_directive_line(cpp);
                    free(parameters);
                    return;
                }
                token = scan_next(cpp);
            }
        }
        token = scan_next(cpp);
    }
    while (token.kind != CPP_NEWLINE && token.kind != CPP_EOF) {
        token.starts_line = 0;
        token.hide = NULL;
        append_token(&replacement, &replacement_len, &replacement_cap, token);
        token = scan_next(cpp);
    }
    cpp_macros_define(cpp->macros, name, function_like, parameters,
                      parameter_count, replacement, replacement_len);
    free(parameters);
    free(replacement);
}

static void undef_macro(Cpp *cpp, SourceLoc directive_loc)
{
    CppToken name = scan_next(cpp);
    CppToken end;

    if (name.kind != CPP_IDENT) {
        diag_error_at(name.kind == CPP_EOF ? directive_loc : name.loc,
                      "macro name must be an identifier");
        if (name.kind != CPP_NEWLINE && name.kind != CPP_EOF)
            consume_directive_line(cpp);
        return;
    }
    end = scan_next(cpp);
    if (end.kind != CPP_NEWLINE && end.kind != CPP_EOF) {
        diag_error_at(end.loc, "extra tokens at end of #undef directive");
        consume_directive_line(cpp);
        return;
    }
    cpp_macros_undef(cpp->macros, name.text);
}

static CppToken *read_directive_line(Cpp *cpp, size_t *out_len)
{
    CppToken *tokens = NULL;
    size_t len = 0;
    size_t cap = 0;
    CppToken token;

    for (;;) {
        token = scan_next(cpp);
        if (token.kind == CPP_NEWLINE || token.kind == CPP_EOF)
            break;
        append_token(&tokens, &len, &cap, token);
    }
    *out_len = len;
    return tokens;
}

static int cpp_active(const Cpp *cpp)
{
    return !cpp->conditionals || cpp->conditionals->branch_active;
}

static void push_conditional(Cpp *cpp, int parent_active, int selected,
                             SourceLoc opening_loc)
{
    Conditional *conditional = arena_alloc_zeroed(sizeof(*conditional));

    conditional->parent_active = parent_active;
    conditional->branch_active = parent_active && selected;
    conditional->branch_taken = parent_active && selected;
    conditional->opening_loc = opening_loc;
    conditional->next = cpp->conditionals;
    cpp->conditionals = conditional;
}

static int resolve_defined(Cpp *cpp, const CppToken *tokens, size_t len,
                           CppToken **out_tokens, size_t *out_len)
{
    CppToken *result = NULL;
    size_t result_len = 0;
    size_t result_cap = 0;

    for (size_t i = 0; i < len; i++) {
        CppToken token = tokens[i];
        CppToken name;
        int parenthesized = 0;

        if (token.kind != CPP_IDENT || !token_is(&token, "defined")) {
            append_token(&result, &result_len, &result_cap, token);
            continue;
        }
        if (++i >= len) {
            diag_error_at(token.loc, "'defined' requires an identifier");
            free(result);
            return 0;
        }
        if (token_is(&tokens[i], "(")) {
            parenthesized = 1;
            if (++i >= len) {
                diag_error_at(token.loc, "'defined' requires an identifier");
                free(result);
                return 0;
            }
        }
        name = tokens[i];
        if (name.kind != CPP_IDENT) {
            diag_error_at(name.loc, "'defined' requires an identifier");
            free(result);
            return 0;
        }
        if (parenthesized && (++i >= len || !token_is(&tokens[i], ")"))) {
            SourceLoc loc = i < len ? tokens[i].loc : name.loc;
            diag_error_at(loc, "expected ')' after 'defined'");
            free(result);
            return 0;
        }
        token.kind = CPP_NUMBER;
        token.text = cpp_macros_defined(cpp->macros, name.text) ? "1" : "0";
        token.len = 1;
        token.hide = NULL;
        append_token(&result, &result_len, &result_cap, token);
    }
    *out_tokens = result;
    *out_len = result_len;
    return 1;
}

static int evaluate_condition(Cpp *cpp, SourceLoc directive_loc,
                              CppToken *tokens, size_t len)
{
    CppToken *resolved = NULL;
    CppToken *expanded = NULL;
    size_t resolved_len = 0;
    size_t expanded_len = 0;
    int selected = 0;

    if (!len) {
        diag_error_at(directive_loc,
                      "expected expression after conditional directive");
        return 0;
    }
    if (!resolve_defined(cpp, tokens, len, &resolved, &resolved_len))
        return 0;
    expanded = cpp_macros_expand_tokens(cpp->macros, resolved, resolved_len,
                                        &expanded_len);
    if (!expanded_len)
        diag_error_at(directive_loc,
                      "expected expression after conditional directive");
    else
        cpp_eval_condition(expanded, expanded_len, &selected);
    free(expanded);
    free(resolved);
    return selected;
}

static void handle_if(Cpp *cpp, CppToken directive)
{
    size_t len;
    CppToken *tokens = read_directive_line(cpp, &len);
    int parent_active = cpp_active(cpp);
    int selected = parent_active
                 ? evaluate_condition(cpp, directive.loc, tokens, len) : 0;

    push_conditional(cpp, parent_active, selected, directive.loc);
    free(tokens);
}

static void handle_ifdef(Cpp *cpp, CppToken directive, int negate)
{
    size_t len;
    CppToken *tokens = read_directive_line(cpp, &len);
    int parent_active = cpp_active(cpp);
    int selected = 0;

    if (parent_active) {
        if (len != 1 || tokens[0].kind != CPP_IDENT) {
            SourceLoc loc = len ? tokens[0].loc : directive.loc;
            diag_error_at(loc, "%s requires an identifier", directive.text);
        } else {
            selected = cpp_macros_defined(cpp->macros, tokens[0].text);
            if (negate)
                selected = !selected;
        }
    }
    push_conditional(cpp, parent_active, selected, directive.loc);
    free(tokens);
}

static void handle_elif(Cpp *cpp, CppToken directive)
{
    size_t len;
    CppToken *tokens = read_directive_line(cpp, &len);
    Conditional *conditional = cpp->conditionals;
    int selected = 0;

    if (!conditional) {
        diag_error_at(directive.loc, "#elif without matching #if");
        free(tokens);
        return;
    }
    if (conditional->saw_else) {
        diag_error_at(directive.loc, "#elif after #else");
        conditional->branch_active = 0;
        free(tokens);
        return;
    }
    if (conditional->parent_active && !conditional->branch_taken)
        selected = evaluate_condition(cpp, directive.loc, tokens, len);
    conditional->branch_active = conditional->parent_active &&
                                 !conditional->branch_taken && selected;
    if (conditional->branch_active)
        conditional->branch_taken = 1;
    free(tokens);
}

static void reject_extra_tokens(CppToken directive, CppToken *tokens,
                                size_t len)
{
    if (len)
        diag_error_at(tokens[0].loc, "extra tokens at end of #%s directive",
                      directive.text);
}

static void handle_else(Cpp *cpp, CppToken directive)
{
    size_t len;
    CppToken *tokens = read_directive_line(cpp, &len);
    Conditional *conditional = cpp->conditionals;

    reject_extra_tokens(directive, tokens, len);
    free(tokens);
    if (!conditional) {
        diag_error_at(directive.loc, "#else without matching #if");
        return;
    }
    if (conditional->saw_else) {
        diag_error_at(directive.loc, "duplicate #else");
        conditional->branch_active = 0;
        return;
    }
    conditional->saw_else = 1;
    conditional->branch_active = conditional->parent_active &&
                                 !conditional->branch_taken;
    conditional->branch_taken = 1;
}

static void handle_endif(Cpp *cpp, CppToken directive)
{
    size_t len;
    CppToken *tokens = read_directive_line(cpp, &len);

    reject_extra_tokens(directive, tokens, len);
    free(tokens);
    if (!cpp->conditionals) {
        diag_error_at(directive.loc, "#endif without matching #if");
        return;
    }
    cpp->conditionals = cpp->conditionals->next;
}

static void handle_directive(Cpp *cpp, SourceLoc hash_loc)
{
    CppToken directive = scan_next(cpp);
    int active = cpp_active(cpp);

    if (directive.kind == CPP_NEWLINE || directive.kind == CPP_EOF)
        return; /* C89 null directive. */
    if (directive.kind != CPP_IDENT) {
        if (active)
            diag_error_at(directive.loc, "invalid preprocessing directive");
        consume_directive_line(cpp);
        return;
    }
    if (token_is(&directive, "if")) {
        handle_if(cpp, directive);
        return;
    }
    if (token_is(&directive, "ifdef")) {
        handle_ifdef(cpp, directive, 0);
        return;
    }
    if (token_is(&directive, "ifndef")) {
        handle_ifdef(cpp, directive, 1);
        return;
    }
    if (token_is(&directive, "elif")) {
        handle_elif(cpp, directive);
        return;
    }
    if (token_is(&directive, "else")) {
        handle_else(cpp, directive);
        return;
    }
    if (token_is(&directive, "endif")) {
        handle_endif(cpp, directive);
        return;
    }
    if (!active) {
        consume_directive_line(cpp);
        return;
    }
    if (token_is(&directive, "define")) {
        define_macro(cpp, directive.loc);
        return;
    }
    if (token_is(&directive, "undef")) {
        undef_macro(cpp, directive.loc);
        return;
    }
    diag_error_at(hash_loc, "preprocessing directives are not yet supported");
    consume_directive_line(cpp);
}

static CppStreamToken stream_next(void *context)
{
    Cpp *cpp = context;

    if (cpp->replay) {
        Replay *entry = cpp->replay;
        cpp->replay = entry->next;
        return entry->token;
    }
    while (cpp->expansions &&
           cpp->expansions->pos == cpp->expansions->len)
        cpp->expansions = cpp->expansions->next;
    if (cpp->expansions)
        return (CppStreamToken){
            cpp->expansions->tokens[cpp->expansions->pos++], 0 };
    return (CppStreamToken){ scan_next(cpp), 1 };
}

static void stream_unget(void *context, CppStreamToken token)
{
    Cpp *cpp = context;
    Replay *entry = arena_alloc(sizeof(*entry));

    entry->token = token;
    entry->next = cpp->replay;
    cpp->replay = entry;
}

static void stream_push(void *context, CppToken *tokens, size_t len)
{
    Cpp *cpp = context;
    Expansion *expansion;

    if (!len)
        return;
    expansion = arena_alloc(sizeof(*expansion));
    expansion->tokens = tokens;
    expansion->len = len;
    expansion->pos = 0;
    expansion->next = cpp->expansions;
    cpp->expansions = expansion;
}

CppToken cpp_next(Cpp *cpp)
{
    CppMacroStream stream = {
        .ctx = cpp,
        .next = stream_next,
        .unget = stream_unget,
        .push = stream_push,
    };

    for (;;) {
        CppStreamToken input = stream.next(stream.ctx);
        CppToken token = input.token;

        if (input.from_source && token.kind == CPP_PUNCT && token.starts_line &&
            token_is(&token, "#")) {
            handle_directive(cpp, token.loc);
            continue;
        }
        if (input.from_source && token.kind == CPP_EOF && cpp->conditionals &&
            !cpp->reported_unterminated) {
            for (Conditional *conditional = cpp->conditionals; conditional;
                 conditional = conditional->next)
                diag_error_at(conditional->opening_loc,
                              "unterminated conditional directive");
            cpp->reported_unterminated = 1;
        }
        if (input.from_source && !cpp_active(cpp) && token.kind != CPP_EOF)
            continue;
        if (cpp_macros_expand(cpp->macros, &stream, input))
            continue;
        return token;
    }
}
