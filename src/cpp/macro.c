/* SPDX-License-Identifier: MIT */
#include "cpp/macro.h"

#include "arena.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>

typedef struct Macro Macro;

struct CppHide {
    Macro *macro;
    CppHide *next;
};

struct Macro {
    const char *name;
    int function_like;
    const char **parameters;
    size_t parameter_count;
    CppToken *replacement;
    size_t replacement_len;
    SourceLoc definition_loc;
    int defined;
    Macro *next;
};

struct CppMacros {
    Macro *head;
};

typedef struct {
    CppToken *raw;
    size_t raw_len;
    CppToken *expanded;
    size_t expanded_len;
    int expanded_ready;
} Argument;

typedef struct {
    CppToken token;
    int placemarker;
} SubstituteToken;

typedef struct Frame {
    CppToken *tokens;
    size_t len;
    size_t pos;
    struct Frame *next;
} Frame;

typedef struct Replay {
    CppStreamToken token;
    struct Replay *next;
} Replay;

typedef struct {
    CppMacros *macros;
    Frame *frames;
    Replay *replay;
} VectorStream;

static const char *const punctuator_spellings[] = {
    "<<=", ">>=", "...", "##", "->", "++", "--", "<<", ">>",
    "<=", ">=", "==", "!=", "&&", "||", "*=", "/=", "%=", "+=",
    "-=", "&=", "^=", "|=", "[", "]", "(", ")", "{", "}", ".",
    "&", "*", "+", "-", "~", "!", "/", "%", "<", ">", "^", "|",
    "?", ":", ";", "=", ",", "#",
};

const char *const *cpp_punctuators(size_t *count)
{
    *count = sizeof(punctuator_spellings) / sizeof(punctuator_spellings[0]);
    return punctuator_spellings;
}

static int token_is(const CppToken *token, const char *text)
{
    return token && token->text && strcmp(token->text, text) == 0;
}

static void append_token(CppToken **tokens, size_t *len, size_t *cap,
                         CppToken token)
{
    if (*len == *cap) {
        size_t new_cap = *cap ? *cap * 2 : 8;
        CppToken *new_tokens = realloc(*tokens,
                                       new_cap * sizeof(*new_tokens));
        if (!new_tokens)
            diag_fatal("out of memory expanding macro");
        *tokens = new_tokens;
        *cap = new_cap;
    }
    (*tokens)[(*len)++] = token;
}

static void append_substitute(SubstituteToken **tokens, size_t *len,
                              size_t *cap, SubstituteToken token)
{
    if (*len == *cap) {
        size_t new_cap = *cap ? *cap * 2 : 8;
        SubstituteToken *new_tokens = realloc(
            *tokens, new_cap * sizeof(*new_tokens));
        if (!new_tokens)
            diag_fatal("out of memory substituting macro arguments");
        *tokens = new_tokens;
        *cap = new_cap;
    }
    (*tokens)[(*len)++] = token;
}

static Macro *find_entry(CppMacros *macros, const char *name)
{
    for (Macro *macro = macros->head; macro; macro = macro->next)
        if (strcmp(macro->name, name) == 0)
            return macro;
    return NULL;
}

static Macro *find_macro(CppMacros *macros, const char *name)
{
    Macro *macro = find_entry(macros, name);
    return macro && macro->defined ? macro : NULL;
}

CppMacros *cpp_macros_create(void)
{
    return arena_alloc_zeroed(sizeof(CppMacros));
}

int cpp_macros_defined(CppMacros *macros, const char *name)
{
    return find_macro(macros, name) != NULL;
}

void cpp_macros_undef(CppMacros *macros, const char *name)
{
    Macro *macro = find_macro(macros, name);
    if (macro)
        macro->defined = 0;
}

static int hide_contains(const CppHide *hide, const Macro *macro)
{
    for (; hide; hide = hide->next)
        if (hide->macro == macro)
            return 1;
    return 0;
}

static CppHide *hide_add(CppHide *hide, Macro *macro)
{
    CppHide *entry;

    if (hide_contains(hide, macro))
        return hide;
    entry = arena_alloc(sizeof(*entry));
    entry->macro = macro;
    entry->next = hide;
    return entry;
}

static CppHide *hide_union(CppHide *left, CppHide *right)
{
    CppHide *result = left;
    for (CppHide *entry = right; entry; entry = entry->next)
        result = hide_add(result, entry->macro);
    return result;
}

static CppHide *hide_intersection(CppHide *left, CppHide *right)
{
    CppHide *result = NULL;
    for (CppHide *entry = left; entry; entry = entry->next)
        if (hide_contains(right, entry->macro))
            result = hide_add(result, entry->macro);
    return result;
}

static int parameter_index(const Macro *macro, const CppToken *token)
{
    if (token->kind != CPP_IDENT)
        return -1;
    for (size_t i = 0; i < macro->parameter_count; i++)
        if (strcmp(macro->parameters[i], token->text) == 0)
            return (int)i;
    return -1;
}

static int replacement_equal(const Macro *macro, int function_like,
                             const CppToken *parameters,
                             size_t parameter_count,
                             const CppToken *replacement,
                             size_t replacement_len)
{
    if (macro->function_like != function_like ||
        macro->parameter_count != parameter_count ||
        macro->replacement_len != replacement_len)
        return 0;
    for (size_t i = 0; i < parameter_count; i++)
        if (strcmp(macro->parameters[i], parameters[i].text) != 0)
            return 0;
    for (size_t i = 0; i < replacement_len; i++) {
        if (macro->replacement[i].kind != replacement[i].kind ||
            strcmp(macro->replacement[i].text, replacement[i].text) != 0)
            return 0;
        if (i > 0 && !!macro->replacement[i].leading_space !=
                     !!replacement[i].leading_space)
            return 0;
    }
    return 1;
}

void cpp_macros_define(CppMacros *macros, CppToken name, int function_like,
                       const CppToken *parameters, size_t parameter_count,
                       const CppToken *replacement, size_t replacement_len)
{
    Macro *macro;
    int invalid = 0;

    for (size_t i = 0; i < parameter_count; i++)
        for (size_t j = 0; j < i; j++)
            if (strcmp(parameters[i].text, parameters[j].text) == 0) {
                diag_error_at(parameters[i].loc,
                              "duplicate macro parameter '%s'",
                              parameters[i].text);
                invalid = 1;
            }
    for (size_t i = 0; i < replacement_len; i++) {
        if (token_is(&replacement[i], "##") &&
            (i == 0 || i + 1 == replacement_len ||
             token_is(&replacement[i - 1], "##") ||
             token_is(&replacement[i + 1], "##"))) {
            diag_error_at(replacement[i].loc,
                          "invalid '##' placement in macro replacement list");
            invalid = 1;
        }
        if (function_like && token_is(&replacement[i], "#")) {
            int found = 0;
            if (i + 1 < replacement_len && replacement[i + 1].kind == CPP_IDENT)
                for (size_t j = 0; j < parameter_count; j++)
                    if (strcmp(replacement[i + 1].text,
                               parameters[j].text) == 0)
                        found = 1;
            if (!found) {
                diag_error_at(replacement[i].loc,
                              "'#' is not followed by a macro parameter");
                invalid = 1;
            }
        }
    }
    if (invalid)
        return;

    macro = find_macro(macros, name.text);
    if (macro) {
        if (!replacement_equal(macro, function_like, parameters,
                               parameter_count, replacement,
                               replacement_len)) {
            diag_error_at(name.loc,
                          "macro '%s' redefined with different replacement",
                          name.text);
            diag_note_at(macro->definition_loc, "previous definition is here");
        }
        return;
    }
    macro = find_entry(macros, name.text);
    if (!macro) {
        macro = arena_alloc_zeroed(sizeof(*macro));
        macro->name = name.text;
        macro->next = macros->head;
        macros->head = macro;
    }
    macro->function_like = function_like;
    macro->parameter_count = parameter_count;
    macro->parameters = arena_alloc((parameter_count ? parameter_count : 1) *
                                    sizeof(*macro->parameters));
    for (size_t i = 0; i < parameter_count; i++)
        macro->parameters[i] = parameters[i].text;
    macro->replacement = arena_alloc((replacement_len ? replacement_len : 1) *
                                     sizeof(*macro->replacement));
    if (replacement_len)
        memcpy(macro->replacement, replacement,
               replacement_len * sizeof(*replacement));
    macro->replacement_len = replacement_len;
    macro->definition_loc = name.loc;
    macro->defined = 1;
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

static int valid_punctuator(const char *text)
{
    size_t count;
    const char *const *punctuators = cpp_punctuators(&count);
    for (size_t i = 0; i < count; i++)
        if (strcmp(text, punctuators[i]) == 0)
            return 1;
    return 0;
}

static CppTokenKind pasted_kind(const char *text)
{
    size_t len = strlen(text);
    size_t i;
    int quote;

    if (!len)
        return CPP_EOF;
    if ((text[0] == 'L' && len > 1 && (text[1] == '\'' || text[1] == '"')) ||
        text[0] == '\'' || text[0] == '"') {
        i = text[0] == 'L' ? 1 : 0;
        quote = text[i++];
        while (i < len) {
            if (text[i] == '\\') {
                i += 2;
                continue;
            }
            if (text[i++] == quote)
                return i == len ? (quote == '\'' ? CPP_CHAR : CPP_STRING)
                                : CPP_EOF;
        }
        return CPP_EOF;
    }
    if (is_ident_start((unsigned char)text[0])) {
        for (i = 1; i < len && is_ident_continue((unsigned char)text[i]); i++)
            ;
        return i == len ? CPP_IDENT : CPP_EOF;
    }
    if ((text[0] >= '0' && text[0] <= '9') ||
        (text[0] == '.' && len > 1 && text[1] >= '0' && text[1] <= '9')) {
        int previous = text[0];
        for (i = 1; i < len; i++) {
            int ch = (unsigned char)text[i];
            if (is_ident_continue(ch) || ch == '.') {
                previous = ch;
                continue;
            }
            if ((ch == '+' || ch == '-') &&
                (previous == 'e' || previous == 'E' ||
                 previous == 'p' || previous == 'P')) {
                previous = ch;
                continue;
            }
            return CPP_EOF;
        }
        return CPP_NUMBER;
    }
    return valid_punctuator(text) ? CPP_PUNCT : CPP_EOF;
}

static CppToken paste_tokens(CppToken left, CppToken right)
{
    size_t len = left.len + right.len;
    char *text = arena_alloc(len + 1);
    CppTokenKind kind;

    memcpy(text, left.text, left.len);
    memcpy(text + left.len, right.text, right.len);
    text[len] = '\0';
    kind = pasted_kind(text);
    if (kind == CPP_EOF) {
        diag_error_at(left.loc,
                      "pasting '%s' and '%s' does not give a valid preprocessing token",
                      left.text, right.text);
        kind = CPP_PUNCT;
    }
    left.kind = kind;
    left.text = text;
    left.len = len;
    left.hide = hide_intersection(left.hide, right.hide);
    return left;
}

static CppToken vector_next(void *context)
{
    VectorStream *stream = context;

    if (stream->replay) {
        Replay *entry = stream->replay;
        stream->replay = entry->next;
        return entry->token.token;
    }
    while (stream->frames && stream->frames->pos == stream->frames->len)
        stream->frames = stream->frames->next;
    if (!stream->frames)
        return (CppToken){ .kind = CPP_EOF };
    return stream->frames->tokens[stream->frames->pos++];
}

static CppStreamToken vector_stream_next(void *context)
{
    CppStreamToken token = { vector_next(context), 0 };
    return token;
}

static void vector_unget(void *context, CppStreamToken token)
{
    VectorStream *stream = context;
    Replay *entry = arena_alloc(sizeof(*entry));
    entry->token = token;
    entry->next = stream->replay;
    stream->replay = entry;
}

static void vector_push(void *context, CppToken *tokens, size_t len)
{
    VectorStream *stream = context;
    if (!len)
        return;
    Frame *frame = arena_alloc(sizeof(*frame));
    frame->tokens = tokens;
    frame->len = len;
    frame->pos = 0;
    frame->next = stream->frames;
    stream->frames = frame;
}

static CppToken *copy_to_arena(const CppToken *tokens, size_t len)
{
    CppToken *copy = arena_alloc((len ? len : 1) * sizeof(*copy));
    if (len)
        memcpy(copy, tokens, len * sizeof(*copy));
    return copy;
}

static CppToken *expand_argument(CppMacros *macros, Argument *argument,
                                 size_t *out_len)
{
    if (!argument->expanded_ready) {
        argument->expanded = cpp_macros_expand_tokens(
            macros, argument->raw, argument->raw_len,
            &argument->expanded_len);
        argument->expanded_ready = 1;
    }
    *out_len = argument->expanded_len;
    return argument->expanded;
}

static CppToken stringify_argument(Argument *argument, SourceLoc loc,
                                   CppHide *hide)
{
    size_t needed = 3;
    size_t at = 0;
    char *text;

    for (size_t i = 0; i < argument->raw_len; i++) {
        if (i > 0 && argument->raw[i].leading_space)
            needed++;
        for (size_t j = 0; j < argument->raw[i].len; j++) {
            int ch = (unsigned char)argument->raw[i].text[j];
            if ((argument->raw[i].kind == CPP_STRING ||
                 argument->raw[i].kind == CPP_CHAR) &&
                (ch == '\\' || ch == '"'))
                needed++;
            needed++;
        }
    }
    text = arena_alloc(needed);
    text[at++] = '"';
    for (size_t i = 0; i < argument->raw_len; i++) {
        if (i > 0 && argument->raw[i].leading_space)
            text[at++] = ' ';
        for (size_t j = 0; j < argument->raw[i].len; j++) {
            int ch = (unsigned char)argument->raw[i].text[j];
            if ((argument->raw[i].kind == CPP_STRING ||
                 argument->raw[i].kind == CPP_CHAR) &&
                (ch == '\\' || ch == '"'))
                text[at++] = '\\';
            text[at++] = (char)ch;
        }
    }
    text[at++] = '"';
    text[at] = '\0';
    return (CppToken){ .kind = CPP_STRING, .text = text, .len = at,
                       .loc = loc, .hide = hide };
}

static int collect_arguments(Macro *macro, CppMacroStream *stream,
                             Argument **out_arguments, CppToken *out_close)
{
    Argument *arguments = NULL;
    size_t argument_count = 0;
    size_t argument_cap = 0;
    CppToken *current = NULL;
    size_t current_len = 0;
    size_t current_cap = 0;
    int depth = 0;
    int pending_space = 0;

    for (;;) {
        CppStreamToken input = stream->next(stream->ctx);
        CppToken token = input.token;

        if (token.kind == CPP_EOF) {
            diag_error_at(token.loc, "unterminated macro invocation");
            goto fail;
        }
        if (token.kind == CPP_NEWLINE) {
            pending_space = 1;
            continue;
        }
        if (token_is(&token, "(") ) {
            depth++;
        } else if (token_is(&token, ")")) {
            if (depth == 0) {
                *out_close = token;
                if (argument_count == 0 && current_len == 0 &&
                    macro->parameter_count == 0) {
                    free(current);
                    *out_arguments = NULL;
                    return 1;
                }
                if (argument_count == argument_cap) {
                    size_t new_cap = argument_cap ? argument_cap * 2 : 4;
                    Argument *new_args = realloc(
                        arguments, new_cap * sizeof(*new_args));
                    if (!new_args)
                        diag_fatal("out of memory collecting macro arguments");
                    arguments = new_args;
                    argument_cap = new_cap;
                }
                arguments[argument_count++] = (Argument){
                    .raw = current, .raw_len = current_len };
                current = NULL;
                break;
            }
            depth--;
        } else if (token_is(&token, ",") && depth == 0) {
            if (argument_count == argument_cap) {
                size_t new_cap = argument_cap ? argument_cap * 2 : 4;
                Argument *new_args = realloc(
                    arguments, new_cap * sizeof(*new_args));
                if (!new_args)
                    diag_fatal("out of memory collecting macro arguments");
                arguments = new_args;
                argument_cap = new_cap;
            }
            arguments[argument_count++] = (Argument){
                .raw = current, .raw_len = current_len };
            current = NULL;
            current_len = 0;
            current_cap = 0;
            pending_space = 0;
            continue;
        }
        if (pending_space)
            token.leading_space = 1;
        pending_space = 0;
        append_token(&current, &current_len, &current_cap, token);
    }
    if (argument_count != macro->parameter_count) {
        diag_error_at(out_close->loc,
                      "macro '%s' requires %zu arguments, but %zu given",
                      macro->name, macro->parameter_count, argument_count);
        goto fail;
    }
    *out_arguments = arguments;
    return 1;

fail:
    free(current);
    for (size_t i = 0; i < argument_count; i++) {
        free(arguments[i].raw);
        free(arguments[i].expanded);
    }
    free(arguments);
    return 0;
}

static CppToken *substitute(CppMacros *macros, Macro *macro,
                            CppToken invocation, CppToken close,
                            Argument *arguments, size_t *out_len)
{
    SubstituteToken *substituted = NULL;
    size_t substituted_len = 0;
    size_t substituted_cap = 0;
    CppToken *output = NULL;
    size_t output_len = 0;
    size_t output_cap = 0;
    CppHide *base_hide = hide_add(
        hide_intersection(invocation.hide, close.hide), macro);

    for (size_t i = 0; i < macro->replacement_len; i++) {
        CppToken token = macro->replacement[i];
        int parameter;

        token.loc = invocation.loc;
        token.starts_line = 0;
        if (token_is(&token, "#")) {
            parameter = parameter_index(macro, &macro->replacement[++i]);
            append_substitute(&substituted, &substituted_len,
                              &substituted_cap,
                (SubstituteToken){ stringify_argument(
                    &arguments[parameter], invocation.loc, base_hide), 0 });
            continue;
        }
        parameter = parameter_index(macro, &token);
        if (parameter >= 0) {
            int pasted = (i > 0 && token_is(&macro->replacement[i - 1], "##")) ||
                         (i + 1 < macro->replacement_len &&
                          token_is(&macro->replacement[i + 1], "##"));
            CppToken *argument_tokens;
            size_t argument_len;

            if (pasted) {
                argument_tokens = arguments[parameter].raw;
                argument_len = arguments[parameter].raw_len;
            } else {
                argument_tokens = expand_argument(macros, &arguments[parameter],
                                                  &argument_len);
            }
            if (!argument_len) {
                append_substitute(&substituted, &substituted_len,
                                  &substituted_cap,
                                  (SubstituteToken){ .placemarker = 1 });
            }
            for (size_t j = 0; j < argument_len; j++) {
                CppToken argument_token = argument_tokens[j];
                argument_token.hide = hide_union(argument_token.hide, base_hide);
                argument_token.loc = invocation.loc;
                append_substitute(&substituted, &substituted_len,
                                  &substituted_cap,
                                  (SubstituteToken){ argument_token, 0 });
            }
            continue;
        }
        token.hide = base_hide;
        append_substitute(&substituted, &substituted_len, &substituted_cap,
                          (SubstituteToken){ token, 0 });
    }

    for (size_t i = 0; i < substituted_len; i++) {
        SubstituteToken item = substituted[i];
        if (!token_is(&item.token, "##")) {
            if (!item.placemarker) {
                append_token(&output, &output_len, &output_cap, item.token);
            } else {
                CppToken marker = {0};
                marker.kind = CPP_EOF;
                marker.hide = base_hide;
                append_token(&output, &output_len, &output_cap, marker);
            }
            continue;
        }
        if (i + 1 >= substituted_len || !output_len)
            continue; /* Definition validation already diagnosed this. */
        SubstituteToken right = substituted[++i];
        CppToken *left = &output[output_len - 1];
        int left_marker = left->kind == CPP_EOF && !left->text;
        if (left_marker && right.placemarker) {
            continue;
        } else if (left_marker) {
            *left = right.token;
        } else if (right.placemarker) {
            continue;
        } else {
            *left = paste_tokens(*left, right.token);
            left->hide = hide_union(left->hide, base_hide);
        }
    }

    {
        size_t compacted = 0;
        for (size_t i = 0; i < output_len; i++)
            if (!(output[i].kind == CPP_EOF && !output[i].text))
                output[compacted++] = output[i];
        output_len = compacted;
    }
    if (output_len) {
        output[0].starts_line = invocation.starts_line;
        output[0].leading_space = invocation.leading_space;
    }
    free(substituted);
    *out_len = output_len;
    return output;
}

static CppToken *object_replacement(Macro *macro, CppToken invocation,
                                    size_t *out_len)
{
    CppToken *tokens = copy_to_arena(macro->replacement,
                                     macro->replacement_len);
    CppHide *hide = hide_add(invocation.hide, macro);

    for (size_t i = 0; i < macro->replacement_len; i++) {
        tokens[i].loc = invocation.loc;
        tokens[i].hide = hide;
        tokens[i].starts_line = i == 0 ? invocation.starts_line : 0;
        if (i == 0)
            tokens[i].leading_space = invocation.leading_space;
    }
    /* Object-like ## has no parameters or placemarkers. */
    size_t compacted = 0;
    for (size_t i = 0; i < macro->replacement_len; i++) {
        if (token_is(&tokens[i], "##")) {
            CppToken right = tokens[++i];
            tokens[compacted - 1] = paste_tokens(tokens[compacted - 1], right);
        } else {
            tokens[compacted++] = tokens[i];
        }
    }
    *out_len = compacted;
    return tokens;
}

int cpp_macros_expand(CppMacros *macros, CppMacroStream *stream,
                      CppStreamToken invocation)
{
    Macro *macro;
    CppToken *replacement;
    size_t replacement_len;

    if (invocation.token.kind != CPP_IDENT ||
        !(macro = find_macro(macros, invocation.token.text)) ||
        hide_contains(invocation.token.hide, macro))
        return 0;
    if (!macro->function_like) {
        replacement = object_replacement(macro, invocation.token,
                                         &replacement_len);
        stream->push(stream->ctx, replacement, replacement_len);
        return 1;
    }

    CppStreamToken *looked = NULL;
    size_t looked_len = 0;
    size_t looked_cap = 0;
    CppStreamToken next;
    do {
        next = stream->next(stream->ctx);
        if (looked_len == looked_cap) {
            size_t new_cap = looked_cap ? looked_cap * 2 : 4;
            CppStreamToken *new_tokens = realloc(
                looked, new_cap * sizeof(*new_tokens));
            if (!new_tokens)
                diag_fatal("out of memory looking ahead for macro invocation");
            looked = new_tokens;
            looked_cap = new_cap;
        }
        looked[looked_len++] = next;
    } while (next.token.kind == CPP_NEWLINE);
    if (!token_is(&next.token, "(")) {
        for (size_t i = looked_len; i > 0; i--)
            stream->unget(stream->ctx, looked[i - 1]);
        free(looked);
        return 0;
    }
    free(looked);

    Argument *arguments = NULL;
    CppToken close;
    if (!collect_arguments(macro, stream, &arguments, &close))
        return 1;
    replacement = substitute(macros, macro, invocation.token, close,
                             arguments, &replacement_len);
    for (size_t i = 0; i < macro->parameter_count; i++) {
        free(arguments[i].raw);
        free(arguments[i].expanded);
    }
    free(arguments);
    {
        CppToken *arena_replacement = copy_to_arena(replacement,
                                                    replacement_len);
        free(replacement);
        stream->push(stream->ctx, arena_replacement, replacement_len);
    }
    return 1;
}

CppToken *cpp_macros_expand_tokens(CppMacros *macros,
                                   const CppToken *tokens, size_t len,
                                   size_t *out_len)
{
    VectorStream vector = { .macros = macros };
    Frame base = { copy_to_arena(tokens, len), len, 0, NULL };
    CppMacroStream stream = {
        .ctx = &vector,
        .next = vector_stream_next,
        .unget = vector_unget,
        .push = vector_push,
    };
    CppToken *output = NULL;
    size_t output_len = 0;
    size_t output_cap = 0;

    vector.frames = &base;
    for (;;) {
        CppStreamToken input = stream.next(stream.ctx);
        if (input.token.kind == CPP_EOF)
            break;
        if (cpp_macros_expand(macros, &stream, input))
            continue;
        append_token(&output, &output_len, &output_cap, input.token);
    }
    *out_len = output_len;
    return output;
}
