/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>
#include <string.h>

int main(void)
{
    FILE *f = tmpfile();
    char line[32];
    if (!f)
        return 1;
    if (fputs("hello file\n", f) == EOF)
        return 2;
    rewind(f);
    if (!fgets(line, sizeof(line), f))
        return 3;
    if (fclose(f) != 0)
        return 4;
    return strcmp(line, "hello file\n") != 0;
}
