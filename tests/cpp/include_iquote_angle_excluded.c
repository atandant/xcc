/* expect-error: header 'quote_only.h' not found */
/* cpp-flags: -iquote tests/cpp/fixtures/include/iquote */
#include <quote_only.h>
int main(void) { return 0; }
