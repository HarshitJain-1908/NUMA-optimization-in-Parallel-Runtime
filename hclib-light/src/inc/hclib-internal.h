/*
 * Copyright 2017 Rice University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stddef.h>

#include "hclib.h"
#include "hclib-deque.h"
#include "hclib-atomics.h"

typedef struct finish_t {
    struct finish_t* parent;
    volatile int counter;
} finish_t;

typedef struct hclib_worker_state {
    pthread_t tid; // the pthread associated
    struct finish_t* current_finish;
    deque_t *deque;
    infoList_t *my_info;
    stolen_tasks_t** traced_steals_deque;
    int id; // The id, identify a worker
    long total_push;
    long total_steals;
    //worker metadata
    unsigned int async_counter, steal_counter;
} hclib_worker_state;
