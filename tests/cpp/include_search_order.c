/* expect: 3 */
/* cpp-flags: -Itests/cpp/fixtures/include/search1 -Itests/cpp/fixtures/include/search2 */
#include <order.h>
int main(void) { return ORDER_VALUE; }
