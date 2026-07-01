/* SPDX-License-Identifier: MIT */
/* expect-error: comparison between incompatible pointer types 'int *' and 'char *' */
int main(void) {
    int x;
    int *a;
    char *b;
    void *v;
    a = &x;
    v = &x;
    b = v;
    return a == b;
}
