/* SPDX-License-Identifier: MIT */
#ifndef XCC_LIR_H
#define XCC_LIR_H

#include <stdio.h>
#include "ast.h"

typedef enum {
    LIR_MOVI,
    LIR_MOV,
    LIR_LOAD,
    LIR_STORE,
    LIR_LEA,
    LIR_LEA_SYM,
    LIR_ADD,
    LIR_SUB,
    LIR_MUL,
    LIR_DIV,
    LIR_MOD,
    LIR_SDIV_POW2, /* signed trunc-toward-zero / 2^aux */
    LIR_SMOD_POW2, /* signed trunc-toward-zero % 2^aux */
    LIR_UDIV_POW2, /* unsigned / 2^aux */
    LIR_UMOD_POW2, /* unsigned % 2^aux */
    LIR_AND,
    LIR_OR,
    LIR_SHL,
    LIR_SHR,
    LIR_SAR,
    LIR_NEG,
    LIR_SETCC,
    LIR_BR,
    LIR_JMP,
    LIR_LABEL,
    LIR_CONV,
    LIR_CALL,
    LIR_RET,
    LIR_MEMCPY,   /* a=dst addr, b=src addr, aux=size bytes */
} LirOp;

typedef enum {
    OPND_NONE,
    OPND_VREG,
    OPND_PHYS,
    OPND_IMM,
    OPND_MEM,
} OpndKind;

typedef enum { LIR_W4, LIR_W8 } LirWidth;

typedef enum { LIR_SGN_Z, LIR_SGN_S, LIR_SGN_U } LirSign;

typedef enum {
    CC_EQ,
    CC_NE,
    CC_LT,
    CC_LE,
    CC_GT,
    CC_GE,
} LirCond;

typedef enum {
    CONV_ZEXT8,
    CONV_SEXT8,
    CONV_ZEXT16,
    CONV_SEXT16,
    CONV_ZEXT32,     /* zero-extend low 32 bits to 64 (unsigned widen) */
    CONV_SEXT32_64,
    CONV_TRUNC_LO32,
} ConvKind;

#define LIR_NO_VREG (-1)
#define LIR_FP      (-1)
#define LIR_NO_IDX  (-1)

typedef struct Operand Operand;
struct Operand {
    OpndKind kind;
    union {
        int vreg;
        int phys;
        long imm;
        struct {
            int base;
            long disp;
            int index;
            int scale;
        } mem;
    } u;
};

typedef struct Instr Instr;
struct Instr {
    LirOp op;
    int dst;
    Operand a;
    Operand b;
    LirWidth w;
    LirSign sgn;
    LirCond cc;
    ConvKind conv;
    int label;
    char *call_name;
    int call_indirect;
    int call_reg;
    char *sym_name;
    int nargs;
    int call_nreg;
    Operand *call_args;
    int aux; /* LOAD/STORE: byte width; POW2 ops: log2(divisor) */
};

typedef struct {
    int begin;
    int end;
} LoopRange;

typedef struct {
    int offset;
    int vreg;
} LocalHome;

typedef struct LirFn LirFn;
struct LirFn {
    char *name;
    Instr *instrs;
    int ninstr;
    int cap;
    int nvreg;
    LoopRange *loops;
    int nloops;
    int loops_cap;
    int label_count;
    int epilogue_label;
    LocalHome *homes;
    int nhomes;
    int homes_cap;
};

Operand lir_vreg(int v);
Operand lir_phys(int r);
Operand lir_imm(long imm);
Operand lir_mem(int base, long disp);
Operand lir_mem_idx(int base, int index, int scale, long disp);
Operand lir_none(void);

LirFn *lir_fn_new(const char *name);
int lir_new_vreg(LirFn *fn);
int lir_new_label(LirFn *fn);
void lir_add_loop(LirFn *fn, int begin, int end);
int lir_emit(LirFn *fn, Instr ins);
void lir_dump_fn(LirFn *fn, FILE *out);
int lir_home_vreg(LirFn *lf, int offset);
int lir_is_home_vreg(const LirFn *lf, int vreg);
void lir_bind_home(LirFn *lf, int offset, int vreg);
int lir_max_outgoing(const LirFn *lf);

#endif /* XCC_LIR_H */
