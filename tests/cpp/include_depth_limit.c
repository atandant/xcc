/* expect-error: maximum include depth of 200 exceeded */
#include "fixtures/include/cycle/a.h"
int main(void) { return 0; }
