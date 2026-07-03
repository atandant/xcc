/* expect: 8 */
long f(long p1, long p2, long p3, long p4, long p5, long p6, long p7, long p8) {
    return p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8;
}
int main(void) {
    return (int)f(1L, 1L, 1L, 1L, 1L, 1L, 1L, 1L);
}
