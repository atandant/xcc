/* SPDX-License-Identifier: MIT */
/* expect-stdout: value=42 hex=2a word=ok */
#include <stdio.h>

int main(void)
{
    char out[64];
    sprintf(out, "value=%d hex=%x word=%s", 42, 42, "ok");
    printf("%s\n", out);
    return 0;
}
