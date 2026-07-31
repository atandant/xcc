/* SPDX-License-Identifier: MIT */
#include "cpp/cpp.h"

#include "cpp/expr.h"
#include "cpp/macro.h"

#include "arena.h"
#include "diag.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CPP_INCLUDE_DEPTH_LIMIT 200

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

typedef struct CppInput {
    const SourceFile *source;
    LogicalChar *chars;
    size_t len;
    size_t pos;
    int starts_line;
    int pending_space;
    int in_comment;
    SourceLoc comment_loc;
    Conditional *conditional_base;
    int is_include;
    struct CppInput *previous;
} CppInput;

struct Cpp {
    CppInput *input;
    CppMacros *macros;
    Expansion *expansions;
    Replay *replay;
    Conditional *conditionals;
    const char **include_dirs;
    size_t include_dir_count;
    int include_depth;
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

static CppInput *create_input(const SourceFile *source, CppInput *previous,
                              Conditional *conditional_base, int is_include,
                              int apply_early_phases)
{
    const unsigned char *bytes = source_bytes(source);
    size_t size = source_size(source);
    LogicalChar *phase1 = arena_alloc((size + 1) * sizeof(*phase1));
    LogicalChar *phase2 = arena_alloc((size + 1) * sizeof(*phase2));
    size_t n1 = 0;
    size_t n2 = 0;
    int line = 1;
    int col = 1;
    CppInput *input;

    for (size_t i = 0; i < size;) {
        int ch = bytes[i];
        int replacement;

        if (apply_early_phases && ch == '?' && i + 2 < size &&
            bytes[i + 1] == '?' &&
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
        if (apply_early_phases && phase1[i].ch == '\\' && i + 1 < n1 &&
            phase1[i + 1].ch == '\n') {
            i++;
            continue;
        }
        phase2[n2++] = phase1[i];
    }

    input = arena_alloc_zeroed(sizeof(*input));
    input->source = source;
    input->chars = phase2;
    input->len = n2;
    input->starts_line = 1;
    input->conditional_base = conditional_base;
    input->is_include = is_include;
    input->previous = previous;
    return input;
}

static void append_command_text(char **text, size_t *len, size_t *cap,
                                const char *bytes, size_t count)
{
    if (*len + count + 1 < *len)
        diag_fatal("command-line macro definitions are too large");
    if (*len + count + 1 > *cap) {
        size_t needed = *len + count + 1;
        size_t new_cap = *cap ? *cap : 128;
        char *new_text;

        while (new_cap < needed) {
            if (new_cap > (size_t)-1 / 2)
                new_cap = needed;
            else
                new_cap *= 2;
        }
        new_text = realloc(*text, new_cap);
        if (!new_text)
            diag_fatal("out of memory recording command-line macros");
        *text = new_text;
        *cap = new_cap;
    }
    memcpy(*text + *len, bytes, count);
    *len += count;
    (*text)[*len] = '\0';
}

static SourceFile *create_command_source(const CppAction *actions,
                                         size_t action_count)
{
    char *text = NULL;
    size_t len = 0;
    size_t cap = 0;

    for (size_t i = 0; i < action_count; i++) {
        const char *operand = actions[i].operand;
        const char *equals;

        if (strchr(operand, '\n') || strchr(operand, '\r')) {
            diag_error("newline in command-line macro option");
            append_command_text(&text, &len, &cap, "\n", 1);
            continue;
        }
        if (actions[i].kind == CPP_ACTION_UNDEF) {
            append_command_text(&text, &len, &cap, "#undef ", 7);
            append_command_text(&text, &len, &cap, operand, strlen(operand));
            append_command_text(&text, &len, &cap, "\n", 1);
            continue;
        }
        append_command_text(&text, &len, &cap, "#define ", 8);
        equals = strchr(operand, '=');
        if (equals) {
            append_command_text(&text, &len, &cap, operand,
                                (size_t)(equals - operand));
            append_command_text(&text, &len, &cap, " ", 1);
            append_command_text(&text, &len, &cap, equals + 1,
                                strlen(equals + 1));
        } else {
            append_command_text(&text, &len, &cap, operand, strlen(operand));
            append_command_text(&text, &len, &cap, " 1", 2);
        }
        append_command_text(&text, &len, &cap, "\n", 1);
    }
    {
        SourceFile *source = source_create((const unsigned char *)text, len,
                                           "<command-line>");
        free(text);
        return source;
    }
}

Cpp *cpp_create(const SourceFile *source, const CppOptions *options)
{
    Cpp *cpp = arena_alloc_zeroed(sizeof(*cpp));

    cpp->input = create_input(source, NULL, NULL, 0, 1);
    cpp->macros = cpp_macros_create();
    cpp->include_depth = 1;
    if (options && options->include_dir_count) {
        size_t bytes = options->include_dir_count * sizeof(*cpp->include_dirs);

        cpp->include_dirs = arena_alloc(bytes);
        memcpy(cpp->include_dirs, options->include_dirs, bytes);
        cpp->include_dir_count = options->include_dir_count;
    }
    if (options && options->action_count) {
        SourceFile *commands = create_command_source(options->actions,
                                                     options->action_count);
        cpp->input = create_input(commands, cpp->input, NULL, 0, 0);
    }
    return cpp;
}

static int peek(const Cpp *cpp, size_t ahead)
{
    const CppInput *input = cpp->input;
    size_t at = input->pos + ahead;
    return at < input->len ? input->chars[at].ch : EOF;
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
    CppInput *input = cpp->input;
    size_t len = end - start;
    char *text = arena_alloc(len + 1);
    CppToken token = {0};

    for (size_t i = 0; i < len; i++)
        text[i] = (char)input->chars[start + i].ch;
    text[len] = '\0';
    token.kind = kind;
    token.text = text;
    token.len = len;
    token.loc = start < input->len
              ? input->chars[start].loc
              : (SourceLoc){ input->source, 1, 1 };
    token.leading_space = leading_space;
    token.starts_line = starts_line;
    return token;
}

static CppToken newline_token(Cpp *cpp, size_t at)
{
    CppInput *input = cpp->input;
    CppToken token = make_token(cpp, CPP_NEWLINE, at, at + 1,
                                input->starts_line, input->pending_space);
    input->pos = at + 1;
    input->starts_line = 1;
    input->pending_space = 0;
    return token;
}

static CppToken scan_next(Cpp *cpp)
{
    CppInput *input = cpp->input;

    for (;;) {
        int ch;
        size_t start;
        int starts_line;
        int leading_space;

        if (input->in_comment) {
            while (input->pos < input->len) {
                if (peek(cpp, 0) == '*' && peek(cpp, 1) == '/') {
                    input->pos += 2;
                    input->in_comment = 0;
                    input->pending_space = 1;
                    break;
                }
                if (peek(cpp, 0) == '\n')
                    return newline_token(cpp, input->pos);
                input->pos++;
            }
            if (input->in_comment) {
                diag_error_at(input->comment_loc, "unterminated comment");
                input->in_comment = 0;
            }
            continue;
        }

        ch = peek(cpp, 0);
        if (ch == EOF)
            return (CppToken){ .kind = CPP_EOF,
                .loc = { input->source,
                         input->len ? input->chars[input->len - 1].loc.line : 1,
                         input->len ? input->chars[input->len - 1].loc.col + 1 : 1 },
                .starts_line = input->starts_line,
                .leading_space = input->pending_space };
        if (ch == ' ' || ch == '\t' || ch == '\v' || ch == '\f') {
            input->pending_space = 1;
            input->pos++;
            continue;
        }
        if (ch == '\n')
            return newline_token(cpp, input->pos);
        if (ch == '/' && peek(cpp, 1) == '*') {
            input->comment_loc = input->chars[input->pos].loc;
            input->pos += 2;
            input->in_comment = 1;
            input->pending_space = 1;
            continue;
        }
        if (ch == '/' && peek(cpp, 1) == '/') {
            input->pending_space = 1;
            input->pos += 2;
            while (input->pos < input->len && peek(cpp, 0) != '\n')
                input->pos++;
            continue;
        }

        start = input->pos;
        starts_line = input->starts_line;
        leading_space = input->pending_space;
        input->starts_line = 0;
        input->pending_space = 0;

        if (is_ident_start(ch)) {
            if (ch == 'L' && (peek(cpp, 1) == '\'' || peek(cpp, 1) == '"')) {
                ch = peek(cpp, 1);
                input->pos += 2;
                while (input->pos < input->len && peek(cpp, 0) != '\n') {
                    int c = peek(cpp, 0);
                    input->pos++;
                    if (c == '\\' && input->pos < input->len)
                        input->pos++;
                    else if (c == ch)
                        break;
                }
                return make_token(cpp, ch == '\'' ? CPP_CHAR : CPP_STRING,
                                  start, input->pos, starts_line, leading_space);
            }
            input->pos++;
            while (is_ident_continue(peek(cpp, 0)))
                input->pos++;
            return make_token(cpp, CPP_IDENT, start, input->pos,
                              starts_line, leading_space);
        }
        if ((ch >= '0' && ch <= '9') ||
            (ch == '.' && peek(cpp, 1) >= '0' && peek(cpp, 1) <= '9')) {
            int previous = 0;
            input->pos++;
            while (input->pos < input->len) {
                int c = peek(cpp, 0);
                if (is_ident_continue(c) || c == '.') {
                    previous = c;
                    input->pos++;
                    continue;
                }
                if ((c == '+' || c == '-') &&
                    (previous == 'e' || previous == 'E' ||
                     previous == 'p' || previous == 'P')) {
                    previous = c;
                    input->pos++;
                    continue;
                }
                break;
            }
            return make_token(cpp, CPP_NUMBER, start, input->pos,
                              starts_line, leading_space);
        }
        if (ch == '\'' || ch == '"') {
            input->pos++;
            while (input->pos < input->len && peek(cpp, 0) != '\n') {
                int c = peek(cpp, 0);
                input->pos++;
                if (c == '\\' && input->pos < input->len)
                    input->pos++;
                else if (c == ch)
                    break;
            }
            return make_token(cpp, ch == '\'' ? CPP_CHAR : CPP_STRING,
                              start, input->pos, starts_line, leading_space);
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
        input->pos += punct_len;
        return make_token(cpp, CPP_PUNCT, start, input->pos,
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

static char *copy_text(const char *text, size_t len)
{
    char *copy = malloc(len + 1);

    if (!copy)
        diag_fatal("out of memory processing #include");
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
}

static int parse_header_operand(const CppToken *tokens, size_t len,
                                char **out_name, int *out_quoted)
{
    if (len == 1 && tokens[0].kind == CPP_STRING && tokens[0].len >= 2 &&
        tokens[0].text[0] == '"' && tokens[0].text[tokens[0].len - 1] == '"') {
        if (tokens[0].len == 2)
            return 0;
        *out_name = copy_text(tokens[0].text + 1, tokens[0].len - 2);
        *out_quoted = 1;
        return 1;
    }
    if (len >= 3 && token_is(&tokens[0], "<") &&
        token_is(&tokens[len - 1], ">")) {
        size_t name_len = 0;
        size_t at = 0;
        char *name;

        for (size_t i = 1; i + 1 < len; i++) {
            if (tokens[i].leading_space)
                name_len++;
            name_len += tokens[i].len;
        }
        if (!name_len)
            return 0;
        name = malloc(name_len + 1);
        if (!name)
            diag_fatal("out of memory processing #include");
        for (size_t i = 1; i + 1 < len; i++) {
            if (tokens[i].leading_space)
                name[at++] = ' ';
            memcpy(name + at, tokens[i].text, tokens[i].len);
            at += tokens[i].len;
        }
        name[at] = '\0';
        *out_name = name;
        *out_quoted = 0;
        return 1;
    }
    return 0;
}

static char *source_directory(const SourceFile *source)
{
    const char *name = source_name(source);
    const char *slash;

    if (name[0] == '<')
        return copy_text(".", 1);
    slash = strrchr(name, '/');
    if (!slash)
        return copy_text(".", 1);
    if (slash == name)
        return copy_text("/", 1);
    return copy_text(name, (size_t)(slash - name));
}

static char *join_path(const char *directory, const char *name)
{
    size_t dir_len = strlen(directory);
    size_t name_len = strlen(name);
    int separator = dir_len != 0 && directory[dir_len - 1] != '/';
    char *path = malloc(dir_len + (size_t)separator + name_len + 1);

    if (!path)
        diag_fatal("out of memory searching for header");
    memcpy(path, directory, dir_len);
    if (separator)
        path[dir_len++] = '/';
    memcpy(path + dir_len, name, name_len + 1);
    return path;
}

static SourceFile *try_header(CppToken operand, const char *path,
                              int *hard_error)
{
    FILE *file = fopen(path, "rb");
    SourceFile *source;

    if (!file) {
        if (errno != ENOENT && errno != ENOTDIR) {
            diag_error_at(operand.loc, "cannot open header '%s': %s",
                          path, strerror(errno));
            *hard_error = 1;
        }
        return NULL;
    }
    source = source_read(file, path);
    fclose(file);
    return source;
}

static SourceFile *find_header(Cpp *cpp, CppToken operand, const char *name,
                               int quoted)
{
    SourceFile *source;
    int hard_error = 0;

    if (name[0] == '/')
        return try_header(operand, name, &hard_error);
    if (quoted) {
        char *directory = source_directory(cpp->input->source);
        char *path = join_path(directory, name);

        source = try_header(operand, path, &hard_error);
        free(path);
        free(directory);
        if (source || hard_error)
            return source;
    }
    for (size_t i = 0; i < cpp->include_dir_count; i++) {
        char *path = join_path(cpp->include_dirs[i], name);

        source = try_header(operand, path, &hard_error);
        free(path);
        if (source || hard_error)
            return source;
    }
    if (!hard_error)
        diag_error_at(operand.loc, "header '%s' not found", name);
    return NULL;
}

static void handle_include(Cpp *cpp, CppToken directive)
{
    size_t len;
    size_t expanded_len = 0;
    CppToken *tokens = read_directive_line(cpp, &len);
    CppToken *expanded = NULL;
    CppToken operand = len ? tokens[0] : directive;
    char *name = NULL;
    int quoted = 0;
    SourceFile *source;

    if (!parse_header_operand(tokens, len, &name, &quoted)) {
        expanded = cpp_macros_expand_tokens(cpp->macros, tokens, len,
                                            &expanded_len);
        if (!parse_header_operand(expanded, expanded_len, &name, &quoted)) {
            diag_error_at(operand.loc, "invalid #include operand");
            free(expanded);
            free(tokens);
            return;
        }
    }
    if (cpp->include_depth >= CPP_INCLUDE_DEPTH_LIMIT) {
        diag_error_at(operand.loc, "maximum include depth of %d exceeded",
                      CPP_INCLUDE_DEPTH_LIMIT);
        free(name);
        free(expanded);
        free(tokens);
        return;
    }
    source = find_header(cpp, operand, name, quoted);
    if (source) {
        cpp->input = create_input(source, cpp->input, cpp->conditionals, 1, 1);
        cpp->include_depth++;
    }
    free(name);
    free(expanded);
    free(tokens);
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

    if (!conditional || conditional == cpp->input->conditional_base) {
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
    if (!conditional || conditional == cpp->input->conditional_base) {
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
    if (!cpp->conditionals ||
        cpp->conditionals == cpp->input->conditional_base) {
        diag_error_at(directive.loc, "#endif without matching #if");
        return;
    }
    cpp->conditionals = cpp->conditionals->next;
}

static void handle_error(Cpp *cpp, CppToken directive)
{
    size_t len;
    CppToken *tokens = read_directive_line(cpp, &len);
    size_t message_len = 6;
    size_t at = 0;
    char *message;

    for (size_t i = 0; i < len; i++) {
        if (i > 0 && tokens[i].leading_space)
            message_len++;
        message_len += tokens[i].len;
    }
    if (len)
        message_len++;
    message = malloc(message_len + 1);
    if (!message)
        diag_fatal("out of memory processing #error directive");
    memcpy(message + at, "#error", 6);
    at += 6;
    if (len)
        message[at++] = ' ';
    for (size_t i = 0; i < len; i++) {
        if (i > 0 && tokens[i].leading_space)
            message[at++] = ' ';
        memcpy(message + at, tokens[i].text, tokens[i].len);
        at += tokens[i].len;
    }
    message[at] = '\0';
    diag_error_at(directive.loc, "%s", message);
    free(message);
    free(tokens);
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
    if (token_is(&directive, "include")) {
        handle_include(cpp, directive);
        return;
    }
    if (token_is(&directive, "error")) {
        handle_error(cpp, directive);
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
        if (input.from_source && token.kind == CPP_EOF) {
            Conditional *base = cpp->input->conditional_base;

            if (cpp->conditionals != base) {
                for (Conditional *conditional = cpp->conditionals;
                     conditional != base; conditional = conditional->next)
                    diag_error_at(conditional->opening_loc,
                                  "unterminated conditional directive");
                cpp->conditionals = base;
            }
            if (cpp->input->previous) {
                int was_include = cpp->input->is_include;
                cpp->input = cpp->input->previous;
                if (was_include)
                    cpp->include_depth--;
                continue;
            }
        }
        if (input.from_source && !cpp_active(cpp) && token.kind != CPP_EOF)
            continue;
        if (cpp_macros_expand(cpp->macros, &stream, input))
            continue;
        return token;
    }
}
