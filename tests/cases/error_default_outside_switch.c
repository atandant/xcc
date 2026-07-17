/* SPDX-License-Identifier: MIT */
/* expect-error: default label not within a switch statement */
int main(void)
{
default:
    return 0;
}
