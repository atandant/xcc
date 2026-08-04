/* SPDX-License-Identifier: MIT */
/* expect: 26 */
struct Inner { int values[2]; };
struct Outer { struct Inner inner; int tail; };
int main(void)
{
    volatile struct Outer object;
    object.inner.values[0] = 3;
    object.inner.values[1] = 8;
    object.tail = 15;
    return object.inner.values[0] + object.inner.values[1] + object.tail;
}
