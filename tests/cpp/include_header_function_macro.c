/* expect: 8 */
#define PARENT_VALUE 1
#include "fixtures/include/state/state.h"
int main(void) { return HEADER_FUNCTION(5); }
