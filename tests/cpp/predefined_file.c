/* expect: 1 */
int main(void) {
    return sizeof(__FILE__) == sizeof("tests/cpp/predefined_file.c");
}
