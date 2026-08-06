/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    FILE *f = tmpfile();
    int a;
    int b;
    int c;
    if (!f)
        return 1;
    if (fputc('a', f) == EOF || putc('b', f) == EOF)
        return 2;
    rewind(f);
    a = fgetc(f);
    b = getc(f);
    if (ungetc(b, f) == EOF)
        return 3;
    c = getc(f);
    fclose(f);
    return a != 'a' || b != 'b' || c != 'b';
}
