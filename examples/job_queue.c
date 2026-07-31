/* Compile-time-configurable job queue.
 *
 * Native-preprocessor features exercised:
 *   - nested quoted includes and #pragma once
 *   - -D configuration with #ifndef defaults and #error validation
 *   - function-like macros, rescanning, stringification, and token pasting
 *   - one X-macro command list generating an enum and two switch statements
 *   - conditional priority behavior selected at compile time
 *
 * Default FIFO build returns 67. A priority build made with
 * -DJOB_QUEUE_ENABLE_PRIORITY=1 returns 159. Add
 * -DJOB_QUEUE_VERBOSE_BUILD=1 for compile-time configuration notes.
 */

#include "job_queue/config.h"
#include "job_queue/commands.h"
#include "job_queue/queue.h"
#include "job_queue/queue.h" /* Deliberately repeated: #pragma once. */

int puts(char *text);

JOB_QUEUE_DECLARE(JobQueue, Job, JOB_QUEUE_CAPACITY)

Job make_job(int command, int left, int right)
{
    Job job;

    job.command = command;
    job.left = left;
    job.right = right;
    return job;
}

int main(void)
{
    JobQueue queue;
    Job job;
    int checksum;
    char *name;

    puts("xcc native-preprocessor job queue");
    puts("capacity=" JOB_QUEUE_STRINGIFY(JOB_QUEUE_CAPACITY));
#if JOB_QUEUE_ENABLE_PRIORITY
    puts("mode=priority");
#else
    puts("mode=fifo");
#endif

    JobQueue_init(&queue);
    JobQueue_push(&queue, make_job(JOB_ADD, 2, 3), 1);
    JobQueue_push(&queue, make_job(JOB_MUL, 4, 5), 5);
    JobQueue_push(&queue, make_job(JOB_SUB, 20, 7), 3);
    JobQueue_push(&queue, make_job(JOB_XOR, 10, 3), 4);
    JobQueue_push(&queue, make_job(JOB_ADD, 6, 8), 2);

    checksum = 0;
    while (JobQueue_pop(&queue, &job)) {
        name = job_command_name(job.command);
        puts(name);
        checksum = checksum * 3 + job_execute(job) + (name[4] & 7);
    }
    return checksum & 255;
}
