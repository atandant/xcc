/* SPDX-License-Identifier: MIT */
#pragma once

#include "config.h"

/* Generate a fixed-capacity queue and a type-prefixed API. */
#define JOB_QUEUE_DECLARE(Name, ItemType, Capacity)                         \
    typedef struct Name {                                                  \
        ItemType items[Capacity];                                          \
        int priorities[Capacity];                                          \
        int count;                                                         \
    } Name;                                                               \
                                                                          \
    void Name##_init(Name *queue)                                         \
    {                                                                     \
        queue->count = 0;                                                  \
    }                                                                     \
                                                                          \
    int Name##_push(Name *queue, ItemType item, int priority)              \
    {                                                                     \
        int at;                                                           \
                                                                          \
        if (queue->count >= Capacity)                                      \
            return 0;                                                     \
        at = queue->count;                                                \
        queue->items[at] = item;                                          \
        queue->priorities[at] = priority;                                 \
        queue->count = at + 1;                                            \
        return 1;                                                         \
    }                                                                     \
                                                                          \
    int Name##_pop(Name *queue, ItemType *item)                            \
    {                                                                     \
        int selected;                                                     \
        int i;                                                            \
                                                                          \
        if (queue->count == 0)                                            \
            return 0;                                                     \
        selected = 0;                                                     \
        if (JOB_QUEUE_ENABLE_PRIORITY) {                                  \
            for (i = 1; i < queue->count; i = i + 1)                     \
                if (queue->priorities[i] > queue->priorities[selected])   \
                    selected = i;                                         \
        }                                                                 \
        *item = queue->items[selected];                                   \
        for (i = selected; i + 1 < queue->count; i = i + 1) {            \
            queue->items[i] = queue->items[i + 1];                        \
            queue->priorities[i] = queue->priorities[i + 1];              \
        }                                                                 \
        queue->count = queue->count - 1;                                  \
        return 1;                                                         \
    }
