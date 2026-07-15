/* SPDX-License-Identifier: MIT */
/* expect-error: continue statement not within loop */
int main(void)
{
    continue;
    return 0;
}
