/* SPDX-License-Identifier: MIT */
/* expect: 53 */
int main(void)
{
    int i;
    int checks;
    int total;

    i = 0;
    checks = 0;
    total = 0;
    do {
        i = i + 1;
        switch (i) {
        case 1:
            continue;
        default:
            total = total + i;
        }
    } while ((checks = checks + 1) < 3);
    return total * 10 + checks;
}
