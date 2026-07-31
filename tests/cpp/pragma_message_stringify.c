/* expect: 8 */
/* expect-note: #pragma message: value=42 */
#define STRINGIFY_RAW(value) #value
#define STRINGIFY(value) STRINGIFY_RAW(value)
#define VALUE 42
#pragma message "value=" STRINGIFY(VALUE)
int main(void) { return 8; }
