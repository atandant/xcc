/* SPDX-License-Identifier: MIT */
/* expect-warning: initializer refers to 'x' before its value is defined */
int main(void) {
    int x = 5;
    {
        int x = x;
        return x == 5;
    }
}
