/* SPDX-License-Identifier: MIT */
#ifndef XCC_SOURCE_H
#define XCC_SOURCE_H

#include <stddef.h>
#include <stdio.h>

typedef struct SourceFile SourceFile;

SourceFile *source_read(FILE *in, const char *name);
const char *source_name(const SourceFile *source);
const unsigned char *source_bytes(const SourceFile *source);
size_t source_size(const SourceFile *source);
const char *source_line(const SourceFile *source, int line);

#endif /* XCC_SOURCE_H */
