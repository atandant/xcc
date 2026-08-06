/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <assert.h>

int main(void)
{
    int value;

    value = 1;
    assert(value == 1);
    return 0;
}
