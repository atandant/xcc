/* Euclidean GCD: return gcd(48, 18) == 6. */
int gcd(int a, int b)
{
    int t;

    while (b != 0) {
        t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int main(void)
{
    return gcd(48, 18);
}
