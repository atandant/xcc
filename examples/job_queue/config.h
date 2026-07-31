/* SPDX-License-Identifier: MIT */
#pragma once

#ifndef JOB_QUEUE_CAPACITY
#define JOB_QUEUE_CAPACITY 8
#endif

#ifndef JOB_QUEUE_ENABLE_PRIORITY
#define JOB_QUEUE_ENABLE_PRIORITY 0
#endif

#ifndef JOB_QUEUE_VERBOSE_BUILD
#define JOB_QUEUE_VERBOSE_BUILD 0
#endif

#if JOB_QUEUE_CAPACITY < 5
#error JOB_QUEUE_CAPACITY must hold at least five jobs
#endif

#if JOB_QUEUE_ENABLE_PRIORITY != 0 && JOB_QUEUE_ENABLE_PRIORITY != 1
#error JOB_QUEUE_ENABLE_PRIORITY must be zero or one
#endif

#if JOB_QUEUE_VERBOSE_BUILD != 0 && JOB_QUEUE_VERBOSE_BUILD != 1
#error JOB_QUEUE_VERBOSE_BUILD must be zero or one
#endif

#define JOB_QUEUE_STRINGIFY_RAW(value) #value
#define JOB_QUEUE_STRINGIFY(value) JOB_QUEUE_STRINGIFY_RAW(value)

#if JOB_QUEUE_VERBOSE_BUILD
#pragma message("job queue capacity=" JOB_QUEUE_STRINGIFY(JOB_QUEUE_CAPACITY))
#if JOB_QUEUE_ENABLE_PRIORITY
#pragma message "job queue mode=priority"
#else
#pragma message "job queue mode=fifo"
#endif
#endif
