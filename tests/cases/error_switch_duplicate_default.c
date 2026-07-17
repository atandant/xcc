/* SPDX-License-Identifier: MIT */
/* expect-error: multiple default labels in one switch */
int main(void)
{
    switch (1) {
    default:
        return 1;
    default:
        return 2;
    }
}
