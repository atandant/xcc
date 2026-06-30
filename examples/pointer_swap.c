/* Swap two ints through pointers; return their sum (30). */
void swap(int *a, int *b)
{
    int t;

    t = *a;
    *a = *b;
    *b = t;
}

int main(void)
{
    int x;
    int y;

    x = 10;
    y = 20;
    swap(&x, &y);
    return x + y;
}
