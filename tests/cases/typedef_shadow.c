/* SPDX-License-Identifier: MIT */
/* expect: 5 */
typedef int T;

int main(void) {
    T x;
    x = 1;
    {
        typedef long T;
        T y;
        y = 4;
        x = x + (int)y;
    }
    return x;
}
