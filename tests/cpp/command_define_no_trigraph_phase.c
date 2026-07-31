/* expect: 47 */
/* cpp-flags: -DCOMMAND_VALUE=??/ */
#define STRINGIFY_RAW(x) #x
#define STRINGIFY(x) STRINGIFY_RAW(x)
int main(void) { return STRINGIFY(COMMAND_VALUE)[2]; }
