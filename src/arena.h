#ifndef XCC_ARENA_H
#define XCC_ARENA_H

#include <stddef.h>

/* A compiler is a batch process: allocate freely, emit, exit.
 * So we bump-allocate and free everything at once. No per-node free. */
void *arena_alloc(size_t n);
char *arena_strdup(const char *s);
void arena_free_all(void);

#endif /* XCC_ARENA_H */
