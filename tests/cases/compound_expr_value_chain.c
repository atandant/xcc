/* expect: 22 */
int main(void)
{
    int a = 2;
    int b = 2;
    int r;
    r = (a += b *= 3);
    return a + b + r;
}
