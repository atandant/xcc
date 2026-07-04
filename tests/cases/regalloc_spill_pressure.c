/* SPDX-License-Identifier: MIT */
int pick(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; }
/* expect: 21 */
int main(void) {
    int x; int y; int z; int w; int u; int v;
    x=1; y=2; z=3; w=4; u=5; v=6;
    return pick(x,y,z,w,u,v);
}
