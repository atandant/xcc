/* SPDX-License-Identifier: MIT */
/* expect: 7 */
typedef int *IntPtr;

int main(void)
{
    int x;
    IntPtr a[2];

    x = 7;
    a[0] = &x;
    return *a[0];
}
