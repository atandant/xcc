/* SPDX-License-Identifier: MIT */
struct S3 { char x[3]; }; struct S5 { char x[5]; };
struct S7 { char x[7]; }; struct S9 { char x[9]; };
struct S15 { char x[15]; }; struct S17 { char x[17]; };
struct S3 ret3(int); struct S5 ret5(int); struct S7 ret7(int);
struct S9 ret9(int); struct S15 ret15(int); struct S17 ret17(int);
int main(void)
{
    struct S3 a=ret3(3); struct S5 b=ret5(5); struct S7 c=ret7(7);
    struct S9 d=ret9(9); struct S15 e=ret15(15); struct S17 f=ret17(17);
    return (a.x[0]!=3)+(a.x[2]!=4)+(b.x[0]!=5)+(b.x[4]!=6)+
           (c.x[0]!=7)+(c.x[6]!=8)+(d.x[0]!=9)+(d.x[8]!=10)+
           (e.x[0]!=15)+(e.x[14]!=16)+(f.x[0]!=17)+(f.x[16]!=18);
}
