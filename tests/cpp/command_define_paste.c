/* expect: 9 */
/* cpp-flags: -DCAT(a,b)=a##b */
int main(void) { int pasted = 9; return CAT(past, ed); }
