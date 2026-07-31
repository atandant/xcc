/* expect: 11 */
#include "fixtures/include/nested/outer.h"
int main(void) { return OUTER_VALUE + INNER_VALUE; }
