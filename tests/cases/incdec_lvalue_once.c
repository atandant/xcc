/* expect: 24 */
int main(void)
{
    int a[2];
    int *p = a;
    a[0] = 3;
    a[1] = 7;
    (*p++)++;
    return (int)(p - a) * 20 + a[0];
}
