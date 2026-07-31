/* expect: 1 */
int main(void) {
    return sizeof(__TIME__) == 9 && __TIME__[2] == ':' &&
           __TIME__[5] == ':';
}
