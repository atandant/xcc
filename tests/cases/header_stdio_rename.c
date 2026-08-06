/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    const char *oldname = "xcc_stdio_old.tmp";
    const char *newname = "xcc_stdio_new.tmp";
    FILE *f;
    int ch;
    remove(oldname);
    remove(newname);
    f = fopen(oldname, "w");
    if (!f)
        return 1;
    fputc('R', f);
    if (fclose(f) != 0 || rename(oldname, newname) != 0)
        return 2;
    f = fopen(newname, "r");
    if (!f)
        return 3;
    ch = fgetc(f);
    fclose(f);
    if (remove(newname) != 0)
        return 4;
    return ch != 'R';
}
