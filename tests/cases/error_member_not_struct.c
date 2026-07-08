/* SPDX-License-Identifier: MIT */
/* expect-error: member reference base type is not a structure or union */
int main(void) {
    int x;
    return x.x;
}
