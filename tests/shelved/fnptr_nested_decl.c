/* SPDX-License-Identifier: MIT */
/*
 * SHELVED — not part of the test manifest; parser/declarator builder cannot
 * yet parse nested function-pointer declarators (see parser.y and ast.c).
 *
 * Legal C89:
 *   int (*(*x)(int))(char);
 *
 * Workaround with typedefs:
 *   typedef int (*fn_t)(char);
 *   fn_t (*x)(int);
 */

#if 0
int (*(*x)(int))(char);

int main(void) {
    return 0;
}
#endif
