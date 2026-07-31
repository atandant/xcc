/* expect: 4 */
enum Values { POSITIVE = +3, COMPLEMENT = ~0 };
int main(void)
{
    int a[POSITIVE];
    a[0] = COMPLEMENT == -1;
    return POSITIVE + a[0];
}
