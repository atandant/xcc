/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) {
    int a[3]; int *p; int n;
    n=0; p=a;
    while (p < a+3) { a[n]=n+1; n=n+1; p=p+1; }
    return a[0]+a[1]+a[2];
}
