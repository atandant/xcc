/* expect: 24 */
int main(void)
{
    int a[2];
    int i = 0;
    int r;
    a[0] = 4;
    a[1] = 9;
    r = (a[i++] += 3);
    return i * 10 + a[0] + r;
}
