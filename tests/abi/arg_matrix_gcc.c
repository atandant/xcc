/* SPDX-License-Identifier: MIT */
struct S1 { char x[1]; }; struct S2 { char x[2]; };
struct S3 { char x[3]; }; struct S4 { char x[4]; };
struct S5 { char x[5]; }; struct S7 { char x[7]; };
struct S8 { char x[8]; }; struct S9 { char x[9]; };
struct S12 { char x[12]; }; struct S15 { char x[15]; };
struct S16 { char x[16]; }; struct S17 { char x[17]; };
struct S24 { char x[24]; }; struct S32 { char x[32]; };
#define END(s, n, a, b) ((s).x[0] != (a) || (s).x[(n)-1] != (b))
int take1(struct S1 s) { return s.x[0] != 1; }
int take2(int a, struct S2 s) { return a != 21 || END(s,2,2,3); }
int take3(int a,int b,int c,int d,int e,struct S3 s)
{ return a+b+c+d+e != 15 || END(s,3,3,4); }
int take4(int a,int b,int c,int d,int e,int f,struct S4 s)
{ return a+b+c+d+e+f != 21 || END(s,4,4,5); }
int take5(struct S5 s,int a) { return a != 22 || END(s,5,5,6); }
int take7(int a,int b,int c,int d,int e,struct S7 s)
{ return a+b+c+d+e != 15 || END(s,7,7,8); }
int take8(int a,int b,int c,int d,int e,int f,struct S8 s)
{ return a+b+c+d+e+f != 21 || END(s,8,8,9); }
int take9(struct S9 s) { return END(s,9,9,10); }
int take12(int a,int b,int c,int d,struct S12 s)
{ return a+b+c+d != 10 || END(s,12,12,13); }
int take15(int a,int b,int c,int d,int e,struct S15 s)
{ return a+b+c+d+e != 15 || END(s,15,15,16); }
int take16(struct S16 s,int a) { return a != 23 || END(s,16,16,17); }
int take17(struct S17 s,int a) { return a != 24 || END(s,17,17,18); }
int take24(int a,struct S24 s) { return a != 25 || END(s,24,24,25); }
int take32(struct S17 a,struct S32 b,int c)
{ return END(a,17,17,18) || END(b,32,32,33) || c != 26; }
