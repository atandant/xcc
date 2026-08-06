/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>
#include <stdarg.h>

static int scan(const char *text, const char *format, ...)
{
    va_list ap;
    int n;
    va_start(ap, format);
    n = vsscanf(text, format, ap);
    va_end(ap);
    return n;
}

int main(void)
{
    int a;
    double b;
    return scan("31 2.25", "%d %lf", &a, &b) != 2 || a != 31 || b != 2.25;
}
