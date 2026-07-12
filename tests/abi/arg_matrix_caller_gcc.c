/* SPDX-License-Identifier: MIT */
struct S1 { char x[1]; }; struct S2 { char x[2]; };
struct S3 { char x[3]; }; struct S4 { char x[4]; };
struct S5 { char x[5]; }; struct S7 { char x[7]; };
struct S8 { char x[8]; }; struct S9 { char x[9]; };
struct S12 { char x[12]; }; struct S15 { char x[15]; };
struct S16 { char x[16]; }; struct S17 { char x[17]; };
struct S24 { char x[24]; }; struct S32 { char x[32]; };
int take1(struct S1); int take2(int, struct S2);
int take3(int, int, int, int, int, struct S3);
int take4(int, int, int, int, int, int, struct S4);
int take5(struct S5, int); int take7(int, int, int, int, int, struct S7);
int take8(int, int, int, int, int, int, struct S8);
int take9(struct S9); int take12(int, int, int, int, struct S12);
int take15(int, int, int, int, int, struct S15);
int take16(struct S16, int); int take17(struct S17, int);
int take24(int, struct S24); int take32(struct S17, struct S32, int);
int main(void)
{
    struct S1 s1; struct S2 s2; struct S3 s3; struct S4 s4;
    struct S5 s5; struct S7 s7; struct S8 s8; struct S9 s9;
    struct S12 s12; struct S15 s15; struct S16 s16;
    struct S17 s17; struct S24 s24; struct S32 s32;
    int bad = 0;
    s1.x[0]=1; s2.x[0]=2; s2.x[1]=3; s3.x[0]=3; s3.x[2]=4;
    s4.x[0]=4; s4.x[3]=5; s5.x[0]=5; s5.x[4]=6;
    s7.x[0]=7; s7.x[6]=8; s8.x[0]=8; s8.x[7]=9;
    s9.x[0]=9; s9.x[8]=10; s12.x[0]=12; s12.x[11]=13;
    s15.x[0]=15; s15.x[14]=16; s16.x[0]=16; s16.x[15]=17;
    s17.x[0]=17; s17.x[16]=18; s24.x[0]=24; s24.x[23]=25;
    s32.x[0]=32; s32.x[31]=33;
    bad = bad || take1(s1) || take2(21,s2);
    bad = bad || take3(1,2,3,4,5,s3) || take4(1,2,3,4,5,6,s4);
    bad = bad || take5(s5,22) || take7(1,2,3,4,5,s7);
    bad = bad || take8(1,2,3,4,5,6,s8) || take9(s9);
    bad = bad || take12(1,2,3,4,s12) || take15(1,2,3,4,5,s15);
    bad = bad || take16(s16,23) || take17(s17,24);
    bad = bad || take24(25,s24) || take32(s17,s32,26);
    return bad;
}
