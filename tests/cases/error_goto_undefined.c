/* SPDX-License-Identifier: MIT */
/* expect-error: label 'missing' used but not defined */
int main(void)
{
    goto missing;
    return 0;
}
