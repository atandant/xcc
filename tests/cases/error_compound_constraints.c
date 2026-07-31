/* expect-error: invalid operands to compound assignment */
int main(void)
{
    int x;
    int *p = &x;
    p *= 2;
    1 += 2;
    return 0;
}
