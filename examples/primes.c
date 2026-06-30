/* Count primes <= 100. Exit status is the count (25). */
int is_prime(int n)
{
    int d;

    if (n < 2)
        return 0;
    d = 2;
    while (d * d <= n) {
        if (n % d == 0)
            return 0;
        d = d + 1;
    }
    return 1;
}

int main(void)
{
    int n;
    int count;

    count = 0;
    n = 2;
    while (n <= 100) {
        if (is_prime(n))
            count = count + 1;
        n = n + 1;
    }
    return count;
}
