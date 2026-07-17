/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void)
{
    short value;
    value = -1;
    switch (value) {
    case -1:
        return 7;
    default:
        return 9;
    }
}
