/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    FILE *f = tmpfile();
    int a;
    double b;
    if (!f)
        return 1;
    if (fprintf(f, "%d %.2f", 73, 1.25) != 7)
        return 2;
    rewind(f);
    if (fscanf(f, "%d %lf", &a, &b) != 2)
        return 3;
    fclose(f);
    return a != 73 || b != 1.25;
}
