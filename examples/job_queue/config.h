/* SPDX-License-Identifier: MIT */
#pragma once

#ifndef JOB_QUEUE_CAPACITY
#define JOB_QUEUE_CAPACITY 8
#endif

#ifndef JOB_QUEUE_ENABLE_PRIORITY
#define JOB_QUEUE_ENABLE_PRIORITY 0
#endif

#if JOB_QUEUE_CAPACITY < 5
#error JOB_QUEUE_CAPACITY must hold at least five jobs
#endif

#if JOB_QUEUE_ENABLE_PRIORITY != 0 && JOB_QUEUE_ENABLE_PRIORITY != 1
#error JOB_QUEUE_ENABLE_PRIORITY must be zero or one
#endif

#define JOB_QUEUE_STRINGIFY_RAW(value) #value
#define JOB_QUEUE_STRINGIFY(value) JOB_QUEUE_STRINGIFY_RAW(value)
