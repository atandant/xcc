/* SPDX-License-Identifier: MIT */
#ifndef XCC_LEXER_H
#define XCC_LEXER_H

#include "cpp/cpp.h"
#include "source.h"

void lexer_set_source(const SourceFile *source, const CppOptions *options);

#endif /* XCC_LEXER_H */
