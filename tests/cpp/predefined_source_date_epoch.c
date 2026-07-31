/* expect: 1 */
/* source-date-epoch: 1 */
int main(void) {
    return sizeof(__DATE__) == 12 && __DATE__[0] == 'J' &&
           __DATE__[4] == ' ' && __DATE__[5] == '1' &&
           __DATE__[7] == '1' && __DATE__[8] == '9' &&
           __DATE__[9] == '7' && __DATE__[10] == '0' &&
           sizeof(__TIME__) == 9 && __TIME__[0] == '0' &&
           __TIME__[1] == '0' && __TIME__[6] == '0' && __TIME__[7] == '1';
}
