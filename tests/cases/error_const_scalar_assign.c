/* SPDX-License-Identifier: MIT */
/* expect-error: assignment to const-qualified object */
int main(void)
{
    const int value = 1;
    value = 2;
    return value;
}
