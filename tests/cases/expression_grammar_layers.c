/* SPDX-License-Identifier: MIT */
/* expect: 31 */
/* Unary, cast, postfix, multiplicative, and additive binding boundaries. */
struct Box { int value; };

int id(int x)
{
    return x;
}

int main(void)
{
    int value;
    int *p;
    int array[2];
    struct Box box;

    value = 5;
    p = &value;
    array[1] = 7;
    box.value = 9;
    return -id(3) + *p * 2 + (int)(long)4 + array[1] + box.value + sizeof *p;
}
