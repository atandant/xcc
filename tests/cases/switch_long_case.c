/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void)
{
    long value;
    value = 4294967296L;
    switch (value) {
    case 4294967296L:
        return 7;
    default:
        return 9;
    }
}
