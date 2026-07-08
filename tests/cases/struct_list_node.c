/* SPDX-License-Identifier: MIT */
/* expect: 16 */
/* Section 2.5: self-referential struct via pointer to same (incomplete) tag. */
struct list { int v; struct list *next; };

int main(void) {
    struct list node;
    return sizeof(node);
}
