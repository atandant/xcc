/* expect: 34 */
/* cpp-flags: -isystem tests/cpp/fixtures/include/system -I tests/cpp/fixtures/include/user */
#include <choice.h>
int main(void) { return INCLUDE_CHOICE_VALUE; }
