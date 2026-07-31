/* expect: 1 */
int main(void) {
    return sizeof(__DATE__) == 12 && __DATE__[3] == ' ' &&
           __DATE__[6] == ' ';
}
