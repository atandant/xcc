/* expect: 187 */
struct Bits {
    unsigned int x : 4;
    unsigned int y : 4;
};
int main(void)
{
    struct Bits a[2];
    struct Bits *p = a;
    a[0].x = 3;
    a[0].y = 7;
    (p++)->x += 5;
    return (int)(p - a) * 100 + a[0].x * 10 + a[0].y;
}
