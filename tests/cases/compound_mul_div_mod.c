/* expect: 8 */
int main(void)
{
    int x = 7;
    x *= 6;
    x /= 5;
    x %= 9;
    return x;
}
