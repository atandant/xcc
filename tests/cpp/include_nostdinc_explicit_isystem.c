/* expect: 8 */
/* cpp-flags: -nostdinc -isystem include */
#include <stddef.h>
int main(void) { return sizeof(size_t); }
