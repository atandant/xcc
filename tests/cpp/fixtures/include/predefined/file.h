int predefined_header_file(void)
{
    return sizeof(__FILE__) ==
           sizeof("tests/cpp/fixtures/include/predefined/file.h");
}
