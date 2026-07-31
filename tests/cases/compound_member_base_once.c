/* expect: 16 */
struct Item { int value; };
int main(void)
{
    struct Item a[2];
    struct Item *p = a;
    a[0].value = 1;
    a[1].value = 9;
    (p++)->value += 5;
    return (int)(p - a) * 10 + a[0].value;
}
