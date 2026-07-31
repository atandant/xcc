/* Tiny stack bytecode interpreter.
 *
 * The program computes 5! using two locals and a loop.  This stresses switch
 * dispatch, branch-heavy control flow, indexed stack/local accesses, and the
 * interpreter state that stays live around every opcode.  main returns 120.
 */

enum Opcode {
    OP_PUSH,
    OP_LOAD,
    OP_STORE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_GT,
    OP_JZ,
    OP_JMP,
    OP_HALT
};

int factorial_program[][2] = {
    {OP_PUSH, 5},
    {OP_STORE, 0},
    {OP_PUSH, 1},
    {OP_STORE, 1},
    {OP_LOAD, 0},
    {OP_PUSH, 1},
    {OP_GT, 0},
    {OP_JZ, 17},
    {OP_LOAD, 1},
    {OP_LOAD, 0},
    {OP_MUL, 0},
    {OP_STORE, 1},
    {OP_LOAD, 0},
    {OP_PUSH, 1},
    {OP_SUB, 0},
    {OP_STORE, 0},
    {OP_JMP, 4},
    {OP_LOAD, 1},
    {OP_HALT, 0}
};

int run_vm(int program[][2])
{
    int stack[32];
    int locals[8];
    int pc;
    int sp;
    int running;
    int op;
    int arg;
    int rhs;

    pc = 0;
    sp = 0;
    running = 1;
    while (running) {
        op = program[pc][0];
        arg = program[pc][1];
        pc = pc + 1;

        switch (op) {
        case OP_PUSH:
            stack[sp] = arg;
            sp = sp + 1;
            break;
        case OP_LOAD:
            stack[sp] = locals[arg];
            sp = sp + 1;
            break;
        case OP_STORE:
            sp = sp - 1;
            locals[arg] = stack[sp];
            break;
        case OP_ADD:
            sp = sp - 1;
            rhs = stack[sp];
            stack[sp - 1] = stack[sp - 1] + rhs;
            break;
        case OP_SUB:
            sp = sp - 1;
            rhs = stack[sp];
            stack[sp - 1] = stack[sp - 1] - rhs;
            break;
        case OP_MUL:
            sp = sp - 1;
            rhs = stack[sp];
            stack[sp - 1] = stack[sp - 1] * rhs;
            break;
        case OP_GT:
            sp = sp - 1;
            rhs = stack[sp];
            stack[sp - 1] = stack[sp - 1] > rhs;
            break;
        case OP_JZ:
            sp = sp - 1;
            if (stack[sp] == 0)
                pc = arg;
            break;
        case OP_JMP:
            pc = arg;
            break;
        case OP_HALT:
            running = 0;
            break;
        }
    }
    return stack[sp - 1];
}

int main(void)
{
    return run_vm(factorial_program);
}
