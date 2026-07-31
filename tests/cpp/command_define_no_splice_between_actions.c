/* expect: 7 */
/* cpp-flags: -DUNUSED=\ -DCOMMAND_VALUE=7 */
int main(void) { return COMMAND_VALUE; }
