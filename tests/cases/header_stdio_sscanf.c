/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>
#include <string.h>

int main(void)
{
    int a;
    long b;
    double c;
    char word[8];
    int n = sscanf("12 345 6.5 yes", "%d %ld %lf %s", &a, &b, &c, word);
    return n != 4 || a != 12 || b != 345L || c != 6.5 || strcmp(word, "yes") != 0;
}
