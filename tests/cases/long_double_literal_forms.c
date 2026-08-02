/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    return 1.25L + 2.75l != 4.0L || 1e3L != 1000.0L ||
           .5L != 0.5L || 5.L != 5.0L;
}
