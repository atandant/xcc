/* SPDX-License-Identifier: MIT */
/* expect-error: switch quantity is not an integer */
int main(void)
{
    int value;
    int *pointer;
    pointer = &value;
    switch (pointer) {
    default:
        return 0;
    }
}
