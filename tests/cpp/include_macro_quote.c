/* expect: 7 */
#define HEADER "fixtures/include/local/value.h"
#include HEADER
int main(void) { return LOCAL_VALUE; }
