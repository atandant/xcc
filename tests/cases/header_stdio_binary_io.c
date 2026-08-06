/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>
#include <string.h>

int main(void)
{
    FILE *f = tmpfile();
    char input[5] = { 0, 1, 2, 127, (char)255 };
    char output[5];
    if (!f)
        return 1;
    if (fwrite(input, 1, 5, f) != 5)
        return 2;
    rewind(f);
    if (fread(output, 1, 5, f) != 5)
        return 3;
    fclose(f);
    return memcmp(input, output, 5) != 0;
}
