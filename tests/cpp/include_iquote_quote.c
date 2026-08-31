/* expect: 31 */
/* cpp-flags: -iquote tests/cpp/fixtures/include/iquote */
#include "quote_only.h"
int main(void) { return QUOTE_ONLY_VALUE; }
