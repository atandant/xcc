/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int choose(int tag, ...);
typedef int (*chooser)(int, ...);

int choose(int tag, ...)
{
    return tag;
}

int main(void)
{
    chooser fn = choose;
    return fn(7, (char)1, (float)2.0);
}
