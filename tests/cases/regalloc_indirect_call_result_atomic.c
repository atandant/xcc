/* SPDX-License-Identifier: MIT */
/* expect: 37 */
typedef void *(*Alloc)(void *, unsigned, unsigned);

struct Context {
    Alloc alloc;
    void *opaque;
};

struct State {
    struct Context *context;
    int value;
};

static struct State storage;

static void *allocate(void *opaque, unsigned count, unsigned size)
{
    if (opaque != 0 || count != 1 || size != sizeof(struct State))
        return 0;
    return &storage;
}

static int initialize(struct Context *context)
{
    struct State *state;

    state = (struct State *)context->alloc(context->opaque, 1,
                                           sizeof(struct State));
    state->context = context;
    state->value = 37;
    return state->value;
}

int main(void)
{
    struct Context context;

    context.alloc = allocate;
    context.opaque = 0;
    return initialize(&context);
}
