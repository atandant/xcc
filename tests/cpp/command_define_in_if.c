/* expect: 6 */
/* cpp-flags: -DFEATURE=3 */
#if FEATURE == 3
int main(void) { return 6; }
#else
int main(void) { return 0; }
#endif
