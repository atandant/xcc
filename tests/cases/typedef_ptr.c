/* SPDX-License-Identifier: MIT */
/* expect: 9 */
typedef int *IntPtr;

int main(void) {
    int a;
    IntPtr p;
    a = 9;
    p = &a;
    return *p;
}
