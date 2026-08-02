/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Iinclude */
#include <assert.h>

int main(void)
{
    int value;

    value = 1;
    assert(value == 1);
    return 0;
}
