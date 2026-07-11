/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'f' */
int f();
int f(char x);

int main(void)
{
    return 0;
}
