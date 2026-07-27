/* SPDX-License-Identifier: MIT */
/* expect: 120 */
static int factorial(int n)
{
    if (n < 2) return 1;
    return n * factorial(n - 1);
}
int main(void) { return factorial(5); }
