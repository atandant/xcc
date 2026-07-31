/* expect: 1 */
/* cpp-flags: -DEMPTY= */
#if EMPTY + 1 == 1
int main(void) { return 1; }
#else
int main(void) { return 2; }
#endif
