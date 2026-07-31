/* expect: 1 */
/* cpp-flags: -U__XCC__ */
#ifdef __XCC__
int main(void) { return 0; }
#else
int main(void) { return 1; }
#endif
