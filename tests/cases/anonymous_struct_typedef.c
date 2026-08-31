/* SPDX-License-Identifier: MIT */
/* expect: 7 */

typedef struct { int value; } Value;

int main(void)
{
    Value value;
    value.value = 7;
    return value.value;
}
