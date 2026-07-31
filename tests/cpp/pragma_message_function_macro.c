/* expect: 4 */
/* expect-note: #pragma message: function macro */
#define MESSAGE_WRAP(value) value
#pragma message MESSAGE_WRAP("function macro")
int main(void) { return 4; }
