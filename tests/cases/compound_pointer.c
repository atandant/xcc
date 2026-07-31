/* expect: 8 */
int main(void)
{
    int a[5];
    int *p = a;
    p += 3;
    p -= 1;
    a[2] = 8;
    return *p;
}
