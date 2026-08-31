/* expect: 38 */
/* expect-no-warning: malformed #pragma message: expected string literal */
/* cpp-flags: -Werror -isystem tests/cpp/fixtures/include/system/nested */
#include <top.h>
int main(void) { return NESTED_SYSTEM_VALUE; }
