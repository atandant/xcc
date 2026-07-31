/* expect: 8 */
/* cpp-flags: -DFEATURE -U FEATURE */
#ifdef FEATURE
int main(void) { return 1; }
#else
int main(void) { return 8; }
#endif
