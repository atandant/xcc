/* SPDX-License-Identifier: MIT */
/* expect-error: too many arguments to function 'id' */
int id(int x);

int main(void) {
    return id(1, 2);
}
