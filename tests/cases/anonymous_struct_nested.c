/* SPDX-License-Identifier: MIT */
/* expect: 11 */

typedef struct { int x; int y; } Point;
typedef struct { Point point; int extra; } Box;

int main(void)
{
    Box box = { { 3, 4 }, 4 };
    return box.point.x + box.point.y + box.extra;
}
