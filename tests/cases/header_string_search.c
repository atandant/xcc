/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <string.h>

int main(void)
{
    char haystack[] = "abcdef";
    char *found;

    if (strchr(haystack, 'd') != haystack + 3)
        return 1;
    if (strrchr(haystack, 'a') != haystack)
        return 2;
    found = strstr(haystack, "cde");
    if (!found || found != haystack + 2)
        return 3;
    if (strspn("  \tabc", " \t") != 3)
        return 4;
    if (strcspn("abc123", "123") != 3)
        return 5;
    return 0;
}
