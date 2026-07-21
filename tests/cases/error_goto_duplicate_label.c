/* SPDX-License-Identifier: MIT */
/* expect-error: duplicate label 'again' */
int main(void)
{
again:
    ;
again:
    return 0;
}
