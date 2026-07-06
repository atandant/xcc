/* 3x3 integer matrix pipeline.
 *
 * Showcases xcc's brace-initializer support:
 *   - unsized multidimensional array with a *flat* initializer  (int A[][3])
 *   - unsized array whose bound is inferred from *nested* braces (int B[][3])
 *   - a 1-D unsized array of a wider scalar type               (long weight[])
 *   - scalar brace initialization                              (int scale = {2})
 *
 * There is no printf/runtime in xcc examples, so main folds the result into a
 * single exit code: (weighted trace of A*B, scaled) taken mod 256.
 */

/* Multiply two 3x3 matrices: out = a * b. */
void matmul3(int a[3][3], int b[3][3], int out[3][3])
{
    int i;
    int j;
    int k;

    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            int sum = {0};          /* scalar brace init */

            for (k = 0; k < 3; k = k + 1)
                sum = sum + a[i][k] * b[k][j];
            out[i][j] = sum;
        }
    }
}

/* Weighted trace: sum of weight[i] * m[i][i]. */
long weighted_trace(int m[3][3], long weight[3])
{
    long acc = {0};                 /* scalar brace init (long) */
    int i;

    for (i = 0; i < 3; i = i + 1)
        acc = acc + weight[i] * (long)m[i][i];
    return acc;
}

int main(void)
{
    /* Unsized outer dimension, filled by a flat initializer (9 -> 3 rows). */
    int A[][3] = {
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };
    /* Unsized outer dimension, filled by nested braces (3 rows). */
    int B[][3] = {
        {1, 0, 2},
        {0, 3, 0},
        {4, 0, 1}
    };
    /* 1-D unsized array of a wider scalar type. */
    long weight[] = {3, 5, 7};
    int scale = {2};                /* scalar brace init */
    int C[3][3];
    long tr;

    matmul3(A, B, C);
    tr = weighted_trace(C, weight);

    return (int)((tr * (long)scale) % 256);
}
