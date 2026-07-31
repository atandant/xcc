/* expect: 12 */
int main(void)
{
    unsigned int x = 3;
    x <<= 4;
    x >>= 2;
    return (int)x;
}
