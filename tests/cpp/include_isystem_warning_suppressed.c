/* expect: 37 */
/* expect-no-warning: malformed #pragma message: expected string literal */
/* cpp-flags: -Werror -isystem tests/cpp/fixtures/include/system */
#include <warning.h>
int main(void) { return SYSTEM_WARNING_VALUE; }
