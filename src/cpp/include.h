/* SPDX-License-Identifier: MIT */
#ifndef XCC_CPP_INCLUDE_H
#define XCC_CPP_INCLUDE_H

#include <stddef.h>

#include "source.h"
#include "token.h"

typedef struct CppIncludeResolver CppIncludeResolver;

typedef struct {
    const char **quote_dirs;
    size_t quote_dir_count;
    const char **include_dirs;
    size_t include_dir_count;
    const char **system_dirs;
    size_t system_dir_count;
    const char *resource_dir;
} CppIncludeOptions;

CppIncludeResolver *cpp_include_resolver_create(
    const SourceFile *root, const CppIncludeOptions *options);
SourceFile *cpp_include_resolve(CppIncludeResolver *resolver,
                                const SourceFile *including,
                                const char *name, int quoted,
                                SourceLoc include_loc);

#endif /* XCC_CPP_INCLUDE_H */
