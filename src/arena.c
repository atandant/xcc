/* SPDX-License-Identifier: MIT */
#include "arena.h"
#include "diag.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Block {
    struct Block *next;
    size_t used;
    size_t cap;
} Block;

static Block *head = NULL;

#define BLOCK_MIN ((size_t)1 << 16)
#define ARENA_DEFAULT_ALIGN 16u

static void arena_die_oom(void)
{
    diag_fatal("out of memory");
}

static void arena_die_overflow(void)
{
    diag_fatal("arena allocation size overflow");
}

static size_t align_up_size(size_t n, size_t align)
{
    size_t mask = align - 1;
    if (align == 0 || (align & mask) != 0)
        arena_die_overflow();
    if (n > SIZE_MAX - mask)
        arena_die_overflow();
    return (n + mask) & ~mask;
}

static unsigned char *block_data(Block *b)
{
    uintptr_t p = (uintptr_t)(b + 1);
    p = (p + ARENA_DEFAULT_ALIGN - 1) & ~((uintptr_t)ARENA_DEFAULT_ALIGN - 1);
    return (unsigned char *)p;
}

static Block *new_block(size_t need)
{
    size_t cap = need > BLOCK_MIN ? need : BLOCK_MIN;
    size_t extra = ARENA_DEFAULT_ALIGN - 1;
    if (cap > SIZE_MAX - sizeof(Block) - extra)
        arena_die_overflow();

    Block *b = malloc(sizeof(Block) + extra + cap);
    if (!b)
        arena_die_oom();
    b->next = head;
    b->used = 0;
    b->cap = cap;
    head = b;
    return b;
}

static void *arena_alloc_align(size_t n, size_t align)
{
    if (align > ARENA_DEFAULT_ALIGN)
        arena_die_overflow();

    size_t used = head ? align_up_size(head->used, align) : 0;
    if (!head || used > head->cap || n > head->cap - used) {
        new_block(n);
        used = 0;
    }

    if (n > SIZE_MAX - used)
        arena_die_overflow();

    void *p = block_data(head) + used;
    head->used = used + n;
    return p;
}

void *arena_alloc(size_t n)
{
    return arena_alloc_align(n, ARENA_DEFAULT_ALIGN);
}

void *arena_alloc_zeroed(size_t n)
{
    void *p = arena_alloc(n);
    memset(p, 0, n);
    return p;
}

char *arena_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = arena_alloc_align(n, 1);
    memcpy(p, s, n);
    return p;
}

void arena_free_all(void)
{
    Block *b = head;
    while (b) {
        Block *next = b->next;
        free(b);
        b = next;
    }
    head = NULL;
}
