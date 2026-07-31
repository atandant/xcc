/* expect: 1 */
#if defined(__XCC__) && defined(__STDC__) && defined(__LINE__) && \
    defined(__FILE__) && defined(__DATE__) && defined(__TIME__)
int main(void) { return 1; }
#else
int main(void) { return 0; }
#endif
