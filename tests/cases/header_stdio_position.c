/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    FILE *f = tmpfile();
    long pos;
    int ch;
    if (!f)
        return 1;
    fputs("abcdef", f);
    if (fseek(f, -2L, SEEK_END) != 0)
        return 2;
    pos = ftell(f);
    ch = fgetc(f);
    rewind(f);
    if (fclose(f) != 0)
        return 3;
    return pos != 4L || ch != 'e';
}
