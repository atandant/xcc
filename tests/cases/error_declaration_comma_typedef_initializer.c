/* SPDX-License-Identifier: MIT */
/* expect-error: typedef 'Second' is initialized */

int main(void)
{
    typedef int First, Second = 1;
    return 0;
}
