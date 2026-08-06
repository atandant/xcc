/* SPDX-License-Identifier: MIT */
/* expect-stdout: 17/4.5/done */
#include <stdio.h>
#include <stdarg.h>

static void print_values(const char *format, ...)
{
    va_list ap;
    char out[64];
    va_start(ap, format);
    vsprintf(out, format, ap);
    va_end(ap);
    puts(out);
}

int main(void)
{
    print_values("%d/%.1f/%s", 17, 4.5, "done");
    return 0;
}
