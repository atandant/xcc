/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int f(int x) {
    {
        int x = 3;
        x = x + 1;
    }
    return x;
}

int main(void) {
    return f(8);
}
