/* SPDX-License-Identifier: MIT */
/* expect-error: break statement not within loop */
int main(void)
{
    break;
    return 0;
}
