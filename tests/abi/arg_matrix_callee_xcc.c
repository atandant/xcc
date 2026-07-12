/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* abi-role: callee */
/* abi-peer: arg_matrix_caller_gcc.c */
struct S1 { char x[1]; }; struct S2 { char x[2]; };
struct S3 { char x[3]; }; struct S4 { char x[4]; };
struct S5 { char x[5]; }; struct S7 { char x[7]; };
struct S8 { char x[8]; }; struct S9 { char x[9]; };
struct S12 { char x[12]; }; struct S15 { char x[15]; };
struct S16 { char x[16]; }; struct S17 { char x[17]; };
struct S24 { char x[24]; }; struct S32 { char x[32]; };
int take1(struct S1 s) { return s.x[0] != 1; }
int take2(int a, struct S2 s) { return (a != 21)+(s.x[0] != 2)+(s.x[1] != 3); }
int take3(int a,int b,int c,int d,int e,struct S3 s)
{ return (a+b+c+d+e != 15)+(s.x[0] != 3)+(s.x[2] != 4); }
int take4(int a,int b,int c,int d,int e,int f,struct S4 s)
{ return (a+b+c+d+e+f != 21)+(s.x[0] != 4)+(s.x[3] != 5); }
int take5(struct S5 s,int a) { return (a != 22)+(s.x[0] != 5)+(s.x[4] != 6); }
int take7(int a,int b,int c,int d,int e,struct S7 s)
{ return (a+b+c+d+e != 15)+(s.x[0] != 7)+(s.x[6] != 8); }
int take8(int a,int b,int c,int d,int e,int f,struct S8 s)
{ return (a+b+c+d+e+f != 21)+(s.x[0] != 8)+(s.x[7] != 9); }
int take9(struct S9 s) { return (s.x[0] != 9)+(s.x[8] != 10); }
int take12(int a,int b,int c,int d,struct S12 s)
{ return (a+b+c+d != 10)+(s.x[0] != 12)+(s.x[11] != 13); }
int take15(int a,int b,int c,int d,int e,struct S15 s)
{ return (a+b+c+d+e != 15)+(s.x[0] != 15)+(s.x[14] != 16); }
int take16(struct S16 s,int a) { return (a != 23)+(s.x[0] != 16)+(s.x[15] != 17); }
int take17(struct S17 s,int a) { return (a != 24)+(s.x[0] != 17)+(s.x[16] != 18); }
int take24(int a,struct S24 s) { return (a != 25)+(s.x[0] != 24)+(s.x[23] != 25); }
int take32(struct S17 a,struct S32 b,int c)
{ return (a.x[0] != 17)+(a.x[16] != 18)+(b.x[0] != 32)+
         (b.x[31] != 33)+(c != 26); }
