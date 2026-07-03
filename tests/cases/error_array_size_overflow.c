/* expect-error: array size overflows */
int main(void) {
    int a[600000000];
    return 0;
}
