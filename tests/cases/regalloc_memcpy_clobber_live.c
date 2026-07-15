/* SPDX-License-Identifier: MIT */
/* expect: 36 */
struct Pair { long first; long second; };

int main(void) {
    int a;
    int b;
    int c;
    struct Pair src;
    struct Pair dst;
    a = 1;
    b = 2;
    c = 3;
    src.first = 10;
    src.second = 20;
    dst = src;
    return a + b + c + dst.first + dst.second;
}
