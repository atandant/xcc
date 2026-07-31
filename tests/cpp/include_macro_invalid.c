/* expect-error: invalid #include operand */
#define NOT_A_HEADER value
#include NOT_A_HEADER
int main(void) { return 0; }
