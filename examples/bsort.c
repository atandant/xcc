/* Bubble sort an int array; return smallest element (3). */
void bsort(int a[], int n)
{
    int i;
    int j;
    int t;

    i = 0;
    while (i < n) {
        j = 0;
        while (j < n - 1 - i) {
            if (a[j] > a[j + 1]) {
                t = a[j];
                a[j] = a[j + 1];
                a[j + 1] = t;
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

int main(void)
{
    int a[5];

    a[0] = 42;
    a[1] = 8;
    a[2] = 15;
    a[3] = 3;
    a[4] = 27;
    bsort(a, 5);
    return a[0];
}
