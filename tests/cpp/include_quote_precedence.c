/* expect: 4 */
/* cpp-flags: -Itests/cpp/fixtures/include/search2 */
#include "quote_precedence.h"
int main(void) { return PRECEDENCE_VALUE; }
