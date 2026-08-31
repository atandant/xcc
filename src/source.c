/* SPDX-License-Identifier: MIT */
#define _XOPEN_SOURCE 700
#include "source.h"

#include "arena.h"
#include "diag.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct SourceFile {
    const char *name;
    int has_stat_identity;
    dev_t device;
    ino_t inode;
    const char *canonical_path;
    SourceLoc included_from;
    int has_include_origin;
    int is_system_header;
    int line_bias;
    unsigned char *bytes;
    size_t size;
    char **lines;
    int nlines;
};

static void append_byte(unsigned char **buf, size_t *len, size_t *cap, int ch)
{
    if (*len == *cap) {
        size_t new_cap = *cap ? *cap * 2 : 4096;
        unsigned char *new_buf;

        if (new_cap < *cap || new_cap == SIZE_MAX)
            diag_fatal("source file is too large");
        new_buf = realloc(*buf, new_cap);
        if (!new_buf)
            diag_fatal("out of memory reading source");
        *buf = new_buf;
        *cap = new_cap;
    }
    (*buf)[(*len)++] = (unsigned char)ch;
}

SourceFile *source_create(const unsigned char *bytes, size_t size,
                          const char *name)
{
    SourceFile *source = arena_alloc_zeroed(sizeof(*source));
    size_t start = 0;
    int nlines = 0;
    int line = 0;

    source->name = arena_strdup(name);
    source->bytes = arena_alloc(size + 1);
    if (size)
        memcpy(source->bytes, bytes, size);
    source->bytes[size] = '\0';
    source->size = size;

    for (size_t i = 0; i < size; i++)
        if (source->bytes[i] == '\n')
            nlines++;
    if (size == 0 || source->bytes[size - 1] != '\n')
        nlines++;
    source->lines = arena_alloc((size_t)nlines * sizeof(*source->lines));
    source->nlines = nlines;

    for (size_t i = 0; i <= size && line < nlines; i++) {
        if (i != size && source->bytes[i] != '\n')
            continue;
        size_t end = i;
        size_t line_len;
        char *text;

        if (end > start && source->bytes[end - 1] == '\r')
            end--;
        line_len = end - start;
        text = arena_alloc(line_len + 1);
        if (line_len)
            memcpy(text, source->bytes + start, line_len);
        text[line_len] = '\0';
        source->lines[line++] = text;
        start = i + 1;
    }
    return source;
}

SourceFile *source_read(FILE *in, const char *name)
{
    unsigned char *tmp = NULL;
    size_t len = 0;
    size_t cap = 0;
    SourceFile *source;
    int ch;

    while ((ch = fgetc(in)) != EOF)
        append_byte(&tmp, &len, &cap, ch);
    if (ferror(in))
        diag_fatal("error reading source");
    source = source_create(tmp, len, name);
    {
        struct stat status;
        int fd = fileno(in);

        if (fd >= 0 && fstat(fd, &status) == 0) {
            source->has_stat_identity = 1;
            source->device = status.st_dev;
            source->inode = status.st_ino;
        }
    }
    if (name && name[0] != '<') {
        char *canonical = realpath(name, NULL);

        if (canonical) {
            source->canonical_path = arena_strdup(canonical);
            free(canonical);
        }
    }
    free(tmp);
    return source;
}

SourceFile *source_include_view(const SourceFile *source, const char *name,
                                SourceLoc included_from, int is_system_header)
{
    SourceFile *view = arena_alloc(sizeof(*view));

    *view = *source;
    view->name = arena_strdup(name);
    view->included_from = included_from;
    view->has_include_origin = 1;
    view->is_system_header = is_system_header;
    return view;
}

SourceFile *source_logical_view(const SourceFile *source, const char *name,
                                int line_bias)
{
    SourceFile *view = arena_alloc(sizeof(*view));

    *view = *source;
    view->name = arena_strdup(name);
    view->line_bias = line_bias;
    return view;
}

const char *source_name(const SourceFile *source)
{
    return source ? source->name : "<unknown>";
}

int source_same_file(const SourceFile *left, const SourceFile *right)
{
    if (!left || !right)
        return 0;
    if (left == right)
        return 1;
    if (left->has_stat_identity && right->has_stat_identity)
        return left->device == right->device && left->inode == right->inode;
    return left->canonical_path && right->canonical_path &&
           strcmp(left->canonical_path, right->canonical_path) == 0;
}

int source_matches_path(const SourceFile *source, const char *path)
{
    struct stat status;
    char *canonical;
    int matches = 0;

    if (!source || !path)
        return 0;
    if (source->has_stat_identity && stat(path, &status) == 0)
        return source->device == status.st_dev && source->inode == status.st_ino;
    if (!source->canonical_path)
        return 0;
    canonical = realpath(path, NULL);
    if (canonical) {
        matches = strcmp(source->canonical_path, canonical) == 0;
        free(canonical);
    }
    return matches;
}

int source_include_origin(const SourceFile *source, SourceLoc *out)
{
    if (!source || !source->has_include_origin)
        return 0;
    if (out)
        *out = source->included_from;
    return 1;
}

int source_is_system_header(const SourceFile *source)
{
    return source && source->is_system_header;
}

const unsigned char *source_bytes(const SourceFile *source)
{
    return source->bytes;
}

size_t source_size(const SourceFile *source)
{
    return source->size;
}

const char *source_line(const SourceFile *source, int line)
{
    int physical_line;

    if (!source)
        return NULL;
    physical_line = line - source->line_bias;
    if (physical_line < 1 || physical_line > source->nlines)
        return NULL;
    return source->lines[physical_line - 1];
}
