/* SPDX-License-Identifier: MIT */
/* expect-error: redeclaration of 'value' */

int main(void)
{
    int value, value;
    return 0;
}
