/* expect-error: invalid operand to unary plus */
int main(void)
{
    int x;
    int *p = &x;
    +p;
    ~p;
    return 0;
}
