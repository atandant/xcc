/* expect: 7 */
/* cpp-flags: -U__XCC__ -D__XCC__=7 */
int main(void) { return __XCC__; }
