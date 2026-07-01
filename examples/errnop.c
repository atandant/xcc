/* xcc-expect-error: pointer error demonstration; compilation is expected to fail.
 *
 * Run: ./xcc examples/errnop.c
 * Or:  ./examples/build.sh   (treats this file as an expected-failure demo)
 *
 * This program collects several common pointer mistakes in one translation unit
 * so you can see xcc's error lines, carets, and notes together. */

/* ---- conflicting function types (two-note diagnostic) ---- */

int bucket_store(int *slots, int *count, int value);

int bucket_store(int *slots, char *count, int value);

/* ---- helpers used below ---- */

char *format_tag(int *value)
{
    return 0;
}

int scan_pair(int *left, int *right)
{
    return *left == *right;
}

/* ---- broken pointer logic ---- */

int errnop_demo(int *pool, void *cursor, int seed)
{
    int n;
    int *head;
    char *tag;
    int **meta;
    int *tail;

    n = seed;
    head = &n;
    tag = &n;                       /* int * -> char * */
    meta = &n;                     /* int * -> int ** */
    tail = pool;

    tag = format_tag(head);

    if (head == tag)                /* incompatible pointer comparison */
        return 1;

    if (head == n)                  /* pointer vs integer */
        return 2;

    if (*cursor)                    /* void * dereference */
        return 3;

    if (scan_pair(head, tag))       /* call: int * vs char * param */
        return 4;

    bucket_store(pool, head, n);    /* call after conflicting prototype */

    return &n;                      /* returning int * from int function */
}

int shadow_errors(void)
{
    int *cursor;
    int *cursor;                    /* redeclaration + note */

    cursor = 0;
    return &(*cursor + 1);          /* address of non-lvalue (addition result) */
}

int main(void)
{
    int buf;
    void *walk;

    buf = 0;
    walk = &buf;
    return errnop_demo(&buf, walk, 7) + shadow_errors();
}
