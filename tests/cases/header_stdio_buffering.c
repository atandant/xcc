/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    FILE *a = tmpfile();
    FILE *b = tmpfile();
    char buffer[BUFSIZ];
    if (!a || !b)
        return 1;
    if (setvbuf(a, buffer, _IOFBF, sizeof(buffer)) != 0)
        return 2;
    setbuf(b, NULL);
    if (fputs("buffered", a) == EOF || fflush(a) != 0)
        return 3;
    if (fputc('x', b) == EOF || fflush(NULL) != 0)
        return 4;
    return fclose(a) != 0 || fclose(b) != 0;
}
