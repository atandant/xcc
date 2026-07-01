/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) {
    int a[3]; int i; int s;
    i=0; s=0;
    while (i < 3) { a[i]=i+1; s=s+a[i]; i=i+1; }
    return s;
}
