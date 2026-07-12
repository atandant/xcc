/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* abi-role: callee */
/* abi-peer: return_matrix_caller_gcc.c */
struct S3 { char x[3]; }; struct S5 { char x[5]; };
struct S7 { char x[7]; }; struct S9 { char x[9]; };
struct S15 { char x[15]; }; struct S17 { char x[17]; };
struct S3 ret3(int n) { struct S3 s; s.x[0]=n; s.x[2]=n+1; return s; }
struct S5 ret5(int n) { struct S5 s; s.x[0]=n; s.x[4]=n+1; return s; }
struct S7 ret7(int n) { struct S7 s; s.x[0]=n; s.x[6]=n+1; return s; }
struct S9 ret9(int n) { struct S9 s; s.x[0]=n; s.x[8]=n+1; return s; }
struct S15 ret15(int n) { struct S15 s; s.x[0]=n; s.x[14]=n+1; return s; }
struct S17 ret17(int n) { struct S17 s; s.x[0]=n; s.x[16]=n+1; return s; }
