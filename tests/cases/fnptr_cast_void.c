/* SPDX-License-Identifier: MIT */
/* expect: 2 */
int g(int x) { return x; }
int main(void) {
    int (*fp)(int) = g;
    void *p = (void *)fp;
    int (*q)(int) = (int (*)(int))p;
    return q(2);
}
