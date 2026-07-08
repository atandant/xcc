/* SPDX-License-Identifier: MIT */
/* expect: 33 */
/* Section 3.4: subscript then member `arr[i].x` */
struct Point { int x; int y; };

int main(void) {
    struct Point a[2];
    a[0].x = 11;
    a[0].y = 22;
    a[1].x = 33;
    a[1].y = 44;
    return a[1].x;
}
