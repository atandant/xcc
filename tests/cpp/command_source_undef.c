/* expect: 5 */
/* cpp-flags: -DFEATURE */
#undef FEATURE
#ifdef FEATURE
int main(void) { return 1; }
#else
int main(void) { return 5; }
#endif
