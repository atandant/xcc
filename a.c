/* rpncalc.c - medium-complexity C89 torture test for xcc.
 *
 * Deliberately exercises: unions, enums, bitfields, function pointers /
 * dispatch tables, a hand-rolled linked-list stack, recursion, manual
 * string handling (no scanf/sscanf), goto-based error cleanup, static
 * function-scope state, and struct-returning functions.
 *
 * Reads whitespace-separated RPN tokens from argv (or a built-in demo
 * program if no args are given) and evaluates them.  Supports:
 *   numbers, + - * / % , variable names (single lowercase letter) for
 *   store/load, "=" to pop value then name and assign, and "p" to print
 *   top of stack without popping.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    TOK_NUM,
    TOK_VAR,
    TOK_OP,
    TOK_ASSIGN,
    TOK_PRINT
} TokKind;

typedef union {
    long num;
    char var;
    char op;
} TokValue;

typedef struct Tok {
    TokKind kind;
    TokValue v;
} Tok;

/* Stack node: a tagged union-ish cell via plain long (RPN calc is integer
 * only), linked list rather than array to stress malloc/free + pointer
 * chasing rather than array indexing. */
typedef struct StackNode {
    long value;
    struct StackNode *next;
} StackNode;

typedef struct {
    StackNode *top;
    int depth;
} Stack;

/* Bitfield-bearing struct: tracks a handful of evaluator flags packed
 * tightly, purely to give xcc a bitfield read/write to chew on. */
typedef struct {
    unsigned int had_error : 1;
    unsigned int saw_assign : 1;
    unsigned int saw_print : 1;
    unsigned int reserved : 5;
} EvalFlags;

static long variables[26];
static int var_set[26];

static void stack_init(Stack *s)
{
    s->top = NULL;
    s->depth = 0;
}

static void stack_push(Stack *s, long value)
{
    StackNode *n = (StackNode *)malloc(sizeof(StackNode));
    if (n == NULL) {
        fprintf(stderr, "rpncalc: out of memory\n");
        exit(1);
    }
    n->value = value;
    n->next = s->top;
    s->top = n;
    s->depth = s->depth + 1;
}

static int stack_pop(Stack *s, long *out)
{
    StackNode *n;

    if (s->top == NULL)
        return 0;
    n = s->top;
    *out = n->value;
    s->top = n->next;
    s->depth = s->depth - 1;
    free(n);
    return 1;
}

/* Recursive helper: sums a stack destructively (used only for the final
 * "checksum" line in the demo), to give recursion + pointer-to-pointer
 * threading a workout. */
static long stack_sum_rec(StackNode *node)
{
    if (node == NULL)
        return 0;
    return node->value + stack_sum_rec(node->next);
}

/* --- operator dispatch table ------------------------------------------ */

typedef long (*BinOpFn)(long, long);

static long op_add(long a, long b) { return a + b; }
static long op_sub(long a, long b) { return a - b; }
static long op_mul(long a, long b) { return a * b; }

static long op_div(long a, long b)
{
    if (b == 0) {
        fprintf(stderr, "rpncalc: division by zero\n");
        exit(1);
    }
    return a / b;
}

static long op_mod(long a, long b)
{
    if (b == 0) {
        fprintf(stderr, "rpncalc: mod by zero\n");
        exit(1);
    }
    return a % b;
}

typedef struct {
    char sym;
    BinOpFn fn;
} OpEntry;

static const OpEntry op_table[] = {
    { '+', op_add },
    { '-', op_sub },
    { '*', op_mul },
    { '/', op_div },
    { '%', op_mod }
};

#define OP_TABLE_LEN (sizeof(op_table) / sizeof(op_table[0]))

static BinOpFn lookup_op(char sym)
{
    unsigned int i;
    for (i = 0; i < OP_TABLE_LEN; i++) {
        if (op_table[i].sym == sym)
            return op_table[i].fn;
    }
    return NULL;
}

/* --- tokenizer ---------------------------------------------------------- */

static int classify(const char *word, Tok *out)
{
    if (word[0] == '=' && word[1] == '\0') {
        out->kind = TOK_ASSIGN;
        return 1;
    }
    if (word[0] == 'p' && word[1] == '\0') {
        out->kind = TOK_PRINT;
        return 1;
    }
    if (lookup_op(word[0]) != NULL && word[1] == '\0') {
        out->kind = TOK_OP;
        out->v.op = word[0];
        return 1;
    }
    if (word[0] >= 'a' && word[0] <= 'z' && word[1] == '\0') {
        out->kind = TOK_VAR;
        out->v.var = word[0];
        return 1;
    }
    {
        char *end;
        long n = strtol(word, &end, 10);
        if (end != word && *end == '\0') {
            out->kind = TOK_NUM;
            out->v.num = n;
            return 1;
        }
    }
    return 0;
}

/* --- evaluator ----------------------------------------------------------- */

static EvalFlags eval_program(char **words, int nwords, Stack *s)
{
    EvalFlags flags;
    int i;

    flags.had_error = 0;
    flags.saw_assign = 0;
    flags.saw_print = 0;
    flags.reserved = 0;

    for (i = 0; i < nwords; i++) {
        Tok t;

        if (!classify(words[i], &t)) {
            fprintf(stderr, "rpncalc: bad token '%s'\n", words[i]);
            flags.had_error = 1;
            goto done;
        }

        switch (t.kind) {
        case TOK_NUM:
            stack_push(s, t.v.num);
            break;

        case TOK_VAR: {
            int idx = t.v.var - 'a';
            int is_assign_target = (i + 1 < nwords) &&
                                   words[i + 1][0] == '=' &&
                                   words[i + 1][1] == '\0';

            if (is_assign_target) {
                /* This var token is only naming an assignment target for
                 * the '=' that follows; don't read its (possibly unset)
                 * current value. */
                break;
            }
            if (!var_set[idx]) {
                fprintf(stderr, "rpncalc: variable '%c' not set\n", t.v.var);
                flags.had_error = 1;
                goto done;
            }
            stack_push(s, variables[idx]);
            break;
        }

        case TOK_OP: {
            long a, b;
            BinOpFn fn = lookup_op(t.v.op);
            if (!stack_pop(s, &b) || !stack_pop(s, &a)) {
                fprintf(stderr, "rpncalc: stack underflow on '%c'\n", t.v.op);
                flags.had_error = 1;
                goto done;
            }
            stack_push(s, fn(a, b));
            break;
        }

        case TOK_ASSIGN: {
            /* Grammar: "<value> <var-name> =". The var token immediately
             * before this one deliberately skipped pushing its value (see
             * TOK_VAR above) so the stack top here is exactly <value>. */
            long value;
            char name;
            if (i == 0 || words[i - 1][1] != '\0' ||
                !(words[i - 1][0] >= 'a' && words[i - 1][0] <= 'z')) {
                fprintf(stderr, "rpncalc: '=' needs a variable name before it\n");
                flags.had_error = 1;
                goto done;
            }
            name = words[i - 1][0];
            if (!stack_pop(s, &value)) {
                fprintf(stderr, "rpncalc: stack underflow on '='\n");
                flags.had_error = 1;
                goto done;
            }
            variables[name - 'a'] = value;
            var_set[name - 'a'] = 1;
            flags.saw_assign = 1;
            break;
        }

        case TOK_PRINT: {
            long top;
            if (!stack_pop(s, &top)) {
                fprintf(stderr, "rpncalc: stack empty, nothing to print\n");
                flags.had_error = 1;
                goto done;
            }
            printf("%ld\n", top);
            stack_push(s, top);
            flags.saw_print = 1;
            break;
        }
        }
    }

done:
    return flags;
}

/* --- driver --------------------------------------------------------------- */

static char *demo_program[] = {
    "3", "4", "+", "p",          /* 7 */
    "2", "x", "=",               /* x = 2 (previous pop path) */
    "x", "5", "*", "p",          /* 10 */
    "10", "3", "%", "p",         /* 1 */
    "6", "7", "*", "x", "+", "p" /* 44 */
};

int main(int argc, char **argv)
{
    Stack s;
    EvalFlags flags;
    int i;

    stack_init(&s);
    for (i = 0; i < 26; i++) {
        variables[i] = 0;
        var_set[i] = 0;
    }

    if (argc > 1) {
        flags = eval_program(argv + 1, argc - 1, &s);
    } else {
        int n = (int)(sizeof(demo_program) / sizeof(demo_program[0]));
        printf("(no args given, running built-in demo)\n");
        flags = eval_program(demo_program, n, &s);
    }

    if (flags.had_error) {
        fprintf(stderr, "rpncalc: evaluation failed\n");
        return 1;
    }

    printf("final stack depth: %d\n", s.depth);
    printf("checksum (sum of remaining stack): %ld\n", stack_sum_rec(s.top));

    while (s.top != NULL) {
        long v;
        stack_pop(&s, &v);
    }

    return 0;
}