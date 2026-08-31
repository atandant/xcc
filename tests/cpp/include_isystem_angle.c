/* expect: 36 */
/* cpp-flags: -isystemtests/cpp/fixtures/include/system */
#include <system_only.h>
int main(void) { return SYSTEM_ONLY_VALUE; }
