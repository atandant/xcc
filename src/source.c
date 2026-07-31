/* SPDX-License-Identifier: MIT */
#include "source.h"

#include "arena.h"
#include "diag.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct SourceFile {
    const char *name;
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

SourceFile *source_read(FILE *in, const char *name)
{
    SourceFile *source = arena_alloc_zeroed(sizeof(*source));
    unsigned char *tmp = NULL;
    size_t len = 0;
    size_t cap = 0;
    size_t start = 0;
    int ch;
    int nlines = 0;
    int line = 0;

    while ((ch = fgetc(in)) != EOF)
        append_byte(&tmp, &len, &cap, ch);
    if (ferror(in))
        diag_fatal("error reading source");

    source->name = arena_strdup(name);
    source->bytes = arena_alloc(len + 1);
    if (len)
        memcpy(source->bytes, tmp, len);
    source->bytes[len] = '\0';
    source->size = len;
    free(tmp);

    for (size_t i = 0; i < len; i++)
        if (source->bytes[i] == '\n')
            nlines++;
    if (len == 0 || source->bytes[len - 1] != '\n')
        nlines++;
    source->lines = arena_alloc((size_t)nlines * sizeof(*source->lines));
    source->nlines = nlines;

    for (size_t i = 0; i <= len && line < nlines; i++) {
        if (i != len && source->bytes[i] != '\n')
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

const char *source_name(const SourceFile *source)
{
    return source ? source->name : "<unknown>";
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
    if (!source || line < 1 || line > source->nlines)
        return NULL;
    return source->lines[line - 1];
}
