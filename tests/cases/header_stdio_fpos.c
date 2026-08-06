/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    FILE *f = tmpfile();
    fpos_t pos;
    int first;
    int second;
    int repeated;
    if (!f)
        return 1;
    fputs("abc", f);
    rewind(f);
    first = fgetc(f);
    if (fgetpos(f, &pos) != 0)
        return 2;
    second = fgetc(f);
    if (fsetpos(f, &pos) != 0)
        return 3;
    repeated = fgetc(f);
    fclose(f);
    return first != 'a' || second != 'b' || repeated != 'b';
}
