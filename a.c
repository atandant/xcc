int nested(int x, int y, int n, int m) {
    int sum = 0;
    int i;
    int j;
    for (i = 0; i < n; i++) {
        for (j = 0; j < m; j++) {
            sum = sum + (x * y);
        }
    }
    return sum;
}