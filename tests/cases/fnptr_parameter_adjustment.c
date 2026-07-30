/* SPDX-License-Identifier: MIT */
/* expect: 23 */
int add_three(int value) { return value + 3; }

int apply(int fn(int), int value)
{
    return fn(value);
}

int main(void) { return apply(add_three, 20); }
