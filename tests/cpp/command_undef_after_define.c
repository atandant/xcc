/* expect: 9 */
/* cpp-flags: -DFEATURE -UFEATURE */
#ifdef FEATURE
int main(void) { return 1; }
#else
int main(void) { return 9; }
#endif
