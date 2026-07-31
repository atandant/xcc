/* SPDX-License-Identifier: MIT */
#pragma once

#include "config.h"

/* One command list generates the enum, evaluator, and readable names. */
#define JOB_COMMAND_LIST(X) \
    X(JOB_ADD, +)           \
    X(JOB_SUB, -)           \
    X(JOB_MUL, *)           \
    X(JOB_XOR, ^)

#define JOB_COMMAND_ENUM(name, op) name,
enum JobCommand {
    JOB_COMMAND_LIST(JOB_COMMAND_ENUM)
    JOB_COMMAND_COUNT
};
#undef JOB_COMMAND_ENUM

typedef struct Job {
    int command;
    int left;
    int right;
} Job;

int job_execute(Job job)
{
    switch (job.command) {
#define JOB_COMMAND_CASE(name, op) case name: return job.left op job.right;
        JOB_COMMAND_LIST(JOB_COMMAND_CASE)
#undef JOB_COMMAND_CASE
    }
    return 0;
}

char *job_command_name(int command)
{
    switch (command) {
#define JOB_COMMAND_NAME(name, op) case name: return #name;
        JOB_COMMAND_LIST(JOB_COMMAND_NAME)
#undef JOB_COMMAND_NAME
    }
    return "JOB_UNKNOWN";
}
