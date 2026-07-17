/* SPDX-License-Identifier: MIT */
/* expect-error: break statement not within loop or switch */
int main(void)
{
    break;
    return 0;
}
