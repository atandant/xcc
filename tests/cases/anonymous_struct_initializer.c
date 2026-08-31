/* SPDX-License-Identifier: MIT */
/* expect: 9 */

typedef struct { int x; int y; } Point;

int main(void)
{
    Point point = { 4, 5 };
    return point.x + point.y;
}
