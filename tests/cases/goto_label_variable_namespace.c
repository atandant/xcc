/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void)
{
    int retry;

    retry = 8;
    goto retry;
    retry = 99;
retry:
    return retry;
}
