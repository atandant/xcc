/* SPDX-License-Identifier: MIT */
/* expect: 132 */
int changed(void) {
    char text[] = "A";
    text[0]++;
    return text[0];
}
int main(void) { return changed() + changed(); }
