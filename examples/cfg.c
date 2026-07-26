int main(void) {
    int res;
    int step_one;
    int step_two;

    res = 0;
    step_one = 0;
    step_two = 0;

    /* Do first action */
    step_one = 1;
    if (step_one == 0) {
        res = 1;
        goto error;
    }

    /* Do second action */
    step_two = 1;
    if (step_two == 0) {
        res = 2;
        goto cleanup_first;
    }

    goto success;

cleanup_first:
    step_one = 0;

error:
    return res;

success:
    res = 0;
    return res;
}
