#include "arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Block {
    struct Block *next;
    size_t used;
    size_t cap;
    char data[];
} Block;

static Block *head = NULL;

#define BLOCK_MIN (1u << 16)

static Block *new_block(size_t need)
{
    size_t cap = need > BLOCK_MIN ? need : BLOCK_MIN;
    Block *b = malloc(sizeof(Block) + cap);
    if (!b) {
        perror("xcc: arena out of memory");
        exit(1);
    }
    b->next = head;
    b->used = 0;
    b->cap = cap;
    head = b;
    return b;
}

void *arena_alloc(size_t n)
{
    n = (n + 15u) & ~((size_t)15u); /* 16-byte align */
    if (!head || head->used + n > head->cap)
        new_block(n);
    void *p = head->data + head->used;
    head->used += n;
    return p;
}

char *arena_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = arena_alloc(n);
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
