/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* abi-role: caller */
/* abi-peer: return_matrix_gcc.c */
struct S3 { char x[3]; }; struct S5 { char x[5]; };
struct S7 { char x[7]; }; struct S9 { char x[9]; };
struct S15 { char x[15]; }; struct S17 { char x[17]; };
struct S3 ret3(int); struct S5 ret5(int); struct S7 ret7(int);
struct S9 ret9(int); struct S15 ret15(int); struct S17 ret17(int);
int check15(struct S15);
int main(void)
{
    struct S3 a; struct S5 b; struct S7 c; struct S9 d;
    struct S15 e; struct S17 f; int bad = 0;
    a=ret3(3); b=ret5(5); c=ret7(7); d=ret9(9);
    e=ret15(15); f=ret17(17);
    bad=bad+(a.x[0]!=3)+(a.x[2]!=4)+(b.x[0]!=5)+(b.x[4]!=6);
    bad=bad+(c.x[0]!=7)+(c.x[6]!=8)+(d.x[0]!=9)+(d.x[8]!=10);
    bad=bad+(e.x[0]!=15)+(e.x[14]!=16)+(f.x[0]!=17)+(f.x[16]!=18);
    bad=bad+(ret3(23).x[2]!=24)+check15(ret15(25));
    return bad;
}
