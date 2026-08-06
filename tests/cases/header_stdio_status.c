/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    FILE *f = tmpfile();
    int ch;
    if (!f)
        return 1;
    fputc('x', f);
    rewind(f);
    ch = fgetc(f);
    if (ch != 'x' || fgetc(f) != EOF || !feof(f) || ferror(f))
        return 2;
    clearerr(f);
    if (feof(f) || ferror(f))
        return 3;
    return fclose(f) != 0;
}
