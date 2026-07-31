/* expect: 15 */
int main(void)
{
    unsigned int x = 0xfffffff0U;
    return (int)~x;
}
