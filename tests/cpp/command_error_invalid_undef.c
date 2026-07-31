/* expect-error: extra tokens at end of #undef directive */
/* cpp-flags: -UFEATURE=1 */
int main(void) { return 0; }
