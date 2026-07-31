/* expect-error: pasting '0x1p' and '+' does not give a valid preprocessing token */
#define CAT(a, b) a##b
int main(void) { return CAT(0x1p, +)2; }
