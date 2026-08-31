/* SPDX-License-Identifier: MIT */
#include "cpp/include.h"

#include "arena.h"
#include "diag.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct CachedSource CachedSource;
struct CachedSource {
    const SourceFile *source;
    CachedSource *next;
};

struct CppIncludeResolver {
    const char **quote_dirs;
    size_t quote_dir_count;
    const char **include_dirs;
    size_t include_dir_count;
    const char **system_dirs;
    size_t system_dir_count;
    const char *resource_dir;
    CachedSource *cache;
};

static const char **copy_dirs(const char **dirs, size_t count)
{
    const char **copy;

    if (!count)
        return NULL;
    copy = arena_alloc(count * sizeof(*copy));
    memcpy(copy, dirs, count * sizeof(*copy));
    return copy;
}

static void cache_source(CppIncludeResolver *resolver,
                         const SourceFile *source)
{
    CachedSource *entry = arena_alloc(sizeof(*entry));

    entry->source = source;
    entry->next = resolver->cache;
    resolver->cache = entry;
}

CppIncludeResolver *cpp_include_resolver_create(
    const SourceFile *root, const CppIncludeOptions *options)
{
    CppIncludeResolver *resolver = arena_alloc_zeroed(sizeof(*resolver));

    if (options) {
        resolver->quote_dirs = copy_dirs(options->quote_dirs,
                                         options->quote_dir_count);
        resolver->quote_dir_count = options->quote_dir_count;
        resolver->include_dirs = copy_dirs(options->include_dirs,
                                           options->include_dir_count);
        resolver->include_dir_count = options->include_dir_count;
        resolver->system_dirs = copy_dirs(options->system_dirs,
                                          options->system_dir_count);
        resolver->system_dir_count = options->system_dir_count;
        resolver->resource_dir = options->resource_dir
                               ? arena_strdup(options->resource_dir) : NULL;
    }
    cache_source(resolver, root);
    return resolver;
}

static char *copy_text(const char *text, size_t len)
{
    char *copy = malloc(len + 1);

    if (!copy)
        diag_fatal("out of memory searching for header");
    memcpy(copy, text, len);
    copy[len] = '\0';
    return copy;
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

static const SourceFile *cached_source(const CppIncludeResolver *resolver,
                                       const char *path)
{
    for (const CachedSource *entry = resolver->cache; entry;
         entry = entry->next) {
        if (source_matches_path(entry->source, path))
            return entry->source;
    }
    return NULL;
}

static SourceFile *try_path(CppIncludeResolver *resolver, SourceLoc include_loc,
                            const char *path, int is_system, int *hard_error)
{
    const SourceFile *cached = cached_source(resolver, path);
    SourceFile *source;
    FILE *file;

    if (cached)
        return source_include_view(cached, path, include_loc, is_system);
    file = fopen(path, "rb");
    if (!file) {
        if (errno != ENOENT && errno != ENOTDIR) {
            diag_error_at(include_loc, "cannot open header '%s': %s",
                          path, strerror(errno));
            *hard_error = 1;
        }
        return NULL;
    }
    source = source_read(file, path);
    fclose(file);
    cache_source(resolver, source);
    return source_include_view(source, path, include_loc, is_system);
}

static SourceFile *search_dirs(CppIncludeResolver *resolver,
                               const char **dirs, size_t count,
                               const char *name, SourceLoc include_loc,
                               int is_system, int *hard_error)
{
    for (size_t i = 0; i < count; i++) {
        char *path = join_path(dirs[i], name);
        SourceFile *source = try_path(resolver, include_loc, path,
                                      is_system, hard_error);

        free(path);
        if (source || *hard_error)
            return source;
    }
    return NULL;
}

SourceFile *cpp_include_resolve(CppIncludeResolver *resolver,
                                const SourceFile *including,
                                const char *name, int quoted,
                                SourceLoc include_loc)
{
    SourceFile *source = NULL;
    int hard_error = 0;

    if (name[0] == '/') {
        source = try_path(resolver, include_loc, name,
                          source_is_system_header(including), &hard_error);
        if (!source && !hard_error)
            diag_error_at(include_loc, "header '%s' not found", name);
        return source;
    }
    if (!source && !hard_error && quoted) {
        char *directory = source_directory(including);
        char *path = join_path(directory, name);

        source = try_path(resolver, include_loc, path,
                          source_is_system_header(including), &hard_error);
        free(path);
        free(directory);
    }
    if (!source && !hard_error && quoted)
        source = search_dirs(resolver, resolver->quote_dirs,
                             resolver->quote_dir_count, name, include_loc,
                             0, &hard_error);
    if (!source && !hard_error)
        source = search_dirs(resolver, resolver->include_dirs,
                             resolver->include_dir_count, name, include_loc,
                             0, &hard_error);
    if (!source && !hard_error)
        source = search_dirs(resolver, resolver->system_dirs,
                             resolver->system_dir_count, name, include_loc,
                             1, &hard_error);
    if (!source && !hard_error && resolver->resource_dir) {
        const char *resource[] = { resolver->resource_dir };
        source = search_dirs(resolver, resource, 1, name, include_loc,
                             1, &hard_error);
    }
    if (!source && !hard_error)
        diag_error_at(include_loc, "header '%s' not found", name);
    return source;
}
