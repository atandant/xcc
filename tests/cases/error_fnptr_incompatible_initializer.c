/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types initializing 'long (*)(int)' with 'int (*)(int)' */
int identity(int value) { return value; }

int main(void)
{
    long (*fn)(int) = identity;
    return fn(0) != 0;
}
