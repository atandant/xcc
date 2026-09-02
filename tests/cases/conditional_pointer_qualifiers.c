/* SPDX-License-Identifier: MIT */
/* expect: 0 */
struct Value {
    int number;
};

int main(void)
{
    struct Value mutable = { 23 };
    struct Value *plain = &mutable;
    const struct Value *fixed = &mutable;
    const struct Value *selected;
    char *text = "ok";
    const char *word;

    selected = mutable.number ? plain : fixed;
    word = mutable.number ? text : "bad";
    return selected->number != 23 || word[0] != 'o';
}
