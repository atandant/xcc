/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void)
{
    int outer;
    int inner;
    int count;

    outer = 0;
    count = 0;
    do {
        inner = 0;
        do {
            count = count + 1;
            inner = inner + 1;
        } while (inner < 2);
        outer = outer + 1;
    } while (outer < 3);
    return count;
}
