/* SPDX-License-Identifier: MIT */
/* expect-error: duplicate case value */
int main(void)
{
    unsigned int value;
    value = 0;
    switch (value) {
    case -1:
        return 1;
    case 0xffffffff:
        return 2;
    }
    return 0;
}
