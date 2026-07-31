/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int score(int p, int q, int r, int s, int t, int u, int v, int w, double a, double b) { return (p+q)+(r+s)+(t+u)+(v+w) + (a < b) + (a <= b) + (b > a) + (b >= a) + (a != b) + (a == 1.0); }
int main(void) { return score(1,2,3,4,5,6,7,8,1.0,2.0); }
