/* expect: 8 */
#if 1
int selected;
#elif 1
#error untaken branch
#endif
int main(void) { return 8; }
