/* SPDX-License-Identifier: MIT */
/* expect-error: too few arguments to function 'add' */
int add(int a, int b);

int main(void) {
    return add(1);
}
