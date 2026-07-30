/* SPDX-License-Identifier: MIT */
/* expect: 17 */
int add_one(int value) { return value + 1; }
int times_two(int value) { return value * 2; }

int main(void)
{
    int choose_first = 0;
    return (choose_first ? add_one : times_two)(8) + 1;
}
