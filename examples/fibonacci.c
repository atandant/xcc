/* Iterative Fibonacci: return fib(10) == 55. */
int fib(int n)
{
    int a;
    int b;
    int i;
    int next;

    if (n <= 0)
        return 0;
    if (n == 1)
        return 1;
    a = 0;
    b = 1;
    i = 2;
    while (i <= n) {
        next = a + b;
        a = b;
        b = next;
        i = i + 1;
    }
    return b;
}

int main(void)
{
    return fib(10);
}
