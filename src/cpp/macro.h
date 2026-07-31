/* SPDX-License-Identifier: MIT */
#ifndef XCC_CPP_MACRO_H
#define XCC_CPP_MACRO_H

#include "cpp/cpp.h"

typedef struct CppMacros CppMacros;

typedef struct {
    CppToken token;
    int from_source;
} CppStreamToken;

typedef struct {
    void *ctx;
    CppStreamToken (*next)(void *ctx);
    void (*unget)(void *ctx, CppStreamToken token);
    void (*push)(void *ctx, CppToken *tokens, size_t len);
} CppMacroStream;

CppMacros *cpp_macros_create(void);
int cpp_macros_defined(CppMacros *macros, const char *name);
void cpp_macros_undef(CppMacros *macros, const char *name);

void cpp_macros_define(CppMacros *macros, CppToken name, int function_like,
                       const CppToken *parameters, size_t parameter_count,
                       const CppToken *replacement, size_t replacement_len);

/* Expand an identifier at the front of stream when it names an eligible
 * object macro or an invoked function macro. Returns one when consumed. */
int cpp_macros_expand(CppMacros *macros, CppMacroStream *stream,
                      CppStreamToken invocation);

/* Expand one bounded token sequence, used for argument prescan and #if. */
CppToken *cpp_macros_expand_tokens(CppMacros *macros,
                                   const CppToken *tokens, size_t len,
                                   size_t *out_len);

/* Shared phase-3 punctuator spellings for scanning and token pasting. */
const char *const *cpp_punctuators(size_t *count);

#endif /* XCC_CPP_MACRO_H */
