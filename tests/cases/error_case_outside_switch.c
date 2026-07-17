/* SPDX-License-Identifier: MIT */
/* expect-error: case label not within a switch statement */
int main(void)
{
case 1:
    return 0;
}
