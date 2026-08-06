/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <locale.h>

int main(void)
{
    struct lconv *lc;

    if (!setlocale(LC_ALL, "C"))
        return 1;
    lc = localeconv();
    if (!lc || !lc->decimal_point)
        return 2;
    if (lc->decimal_point[0] != '.')
        return 3;
    return 0;
}
