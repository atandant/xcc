#include <stdio.h>

int main() {
    int age = 15;

    if (age < 18) {
        goto incomplete; // Jumps directly to the 'incomplete' label
    }

    printf("This line will be skipped.\n");

incomplete: // The label definition
    printf("Process halted: User is underage.\n");
    return 0;
}
