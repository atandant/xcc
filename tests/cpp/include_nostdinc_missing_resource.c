/* expect-error: header 'stddef.h' not found */
/* cpp-flags: -nostdinc */
#include <stddef.h>
int main(void) { return 0; }
