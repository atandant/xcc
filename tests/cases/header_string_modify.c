/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <string.h>

int main(void)
{
    char buf[16] = "ab";
    char *tok;

    strcat(buf, "cd");
    if (strcmp(buf, "abcd") != 0)
        return 1;
    strncpy(buf, "xy", 3);
    if (buf[0] != 'x' || buf[1] != 'y' || buf[2] != '\0')
        return 2;
    tok = strtok(buf, "y");
    if (!tok || strcmp(tok, "x") != 0)
        return 3;
    return 0;
}
