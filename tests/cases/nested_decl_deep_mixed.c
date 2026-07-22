/* SPDX-License-Identifier: MIT */
/* expect: 13 */
int (*retrow(int (*row)[3]))[3] { return row; }
int (*(*get_row_factory(void))(int (*)[3]))[3] { return retrow; }
int main(void)
{
    int row[3];
    row[2] = 13;
    return get_row_factory()(&row)[0][2];
}
