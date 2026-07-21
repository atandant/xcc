/* SPDX-License-Identifier: MIT */
/* expect: 4 */
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
            break;
            inner = inner + 1;
        } while (inner < 3);
        outer = outer + 1;
    } while (outer < 4);
    return count;
}
