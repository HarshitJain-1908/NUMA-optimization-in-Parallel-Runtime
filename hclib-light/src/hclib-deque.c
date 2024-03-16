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

#include "hclib-internal.h"

void dequeInit(deque_t * deq, void * init_value) {
    deq->head = 0;
    deq->tail = 0;
    deq->data = (volatile void **) malloc(sizeof(void*)*INIT_DEQUE_CAPACITY);
    int i=0;
    while(i < INIT_DEQUE_CAPACITY) {
        deq->data[i] = init_value;
        i++;
    }
}

void infoInit(infoList_t * my_info) {
    my_info->head = NULL;
    my_info->tail = NULL;
}

/*
 * push an entry onto the tail of the deque
 */
void dequePush(deque_t* deq, void* entry) {
    int size = deq->tail - deq->head;
    if (size == INIT_DEQUE_CAPACITY) { /* deque looks full */
        /* may not grow the deque if some interleaving steal occur */
        // assert("DEQUE full, increase deque's size" && 0);
    }
    int n = (deq->tail) % INIT_DEQUE_CAPACITY;
    deq->data[n] = entry;

    deq->tail++;
}

void dequeDestroy(deque_t* deq) {
    free(deq->data);
    free(deq);
}

/*
 * the steal protocol
 */
void * dequeSteal(deque_t * deq) {
    int head;
    /* Cannot read deq->data[head] here
     * Can happen that head=tail=0, then the owner of the deq pushes
     * a new task when stealer is here in the code, resulting in head=0, tail=1
     * All other checks down-below will be valid, but the old value of the buffer head
     * would be returned by the steal rather than the new pushed value.
     */
    int tail;
    
    head = deq->head;
    hc_mfence();
    tail = deq->tail;
    if ((tail - head) <= 0) {
        return NULL;
    }

    void * rt = (void *) deq->data[head % INIT_DEQUE_CAPACITY];

    /* compete with other thieves and possibly the owner (if the size == 1) */
    if (hc_cas(&deq->head, head, head + 1)) { /* competing */
        return rt;
    }
    return NULL;
}

/*
 * pop the task out of the deque from the tail
 */
void * dequePop(deque_t * deq) {
    hc_mfence();
    int tail = deq->tail;
    tail--;
    deq->tail = tail;
    hc_mfence();
    int head = deq->head;

    int size = tail - head;
    if (size < 0) {
        deq->tail = deq->head;
        return NULL;
    }
    void * rt = (void*) deq->data[(tail) % INIT_DEQUE_CAPACITY];

    if (size > 0) {
        return rt;
    }

    /* now size == 1, I need to compete with the thieves */
    if (!hc_cas(&deq->head, head, head + 1))
        rt = NULL; /* losing in competition */

    /* now the deque is empty */
    deq->tail = deq->head;
    return rt;
}
