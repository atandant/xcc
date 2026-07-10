/* SPDX-License-Identifier: MIT */
/* expect: 5 */
typedef int (*handler_t)(int);
int twice(int x) { return x * 2; }
int main(void) {
    handler_t h = twice;
    return h(2) + 1;
}
