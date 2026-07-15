/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int pick(int mode, int value)
{
    if (mode == 0) {
        return value;
    } else if (mode == 1) {
        if (value < 0)
            return 0;
        return value + 1;
    } else {
        return value * 2;
    }
}

int main(void)
{
    return pick(1, 10);
}
