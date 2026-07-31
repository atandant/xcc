/* expect: 1 */
#undef __STDC__
#ifdef __STDC__
int main(void) { return 0; }
#else
int main(void) { return 1; }
#endif
