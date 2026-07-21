/* SPDX-License-Identifier: MIT */
/* expect: 7 */
typedef int retry;

int main(void)
{
    goto retry;
retry:
    return 7;
}
