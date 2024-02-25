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

pthread_key_t selfKey;
pthread_once_t selfKeyInitialized = PTHREAD_ONCE_INIT;

hclib_worker_state* workers;
int * worker_id;
int nb_workers;
int not_done;
static double user_specified_timer = 0;
static double benchmark_start_time_stats = 0;

//volatile(??)
static bool tracing_enabled = false;
static bool replay_enabled = false;

double mysecond() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + ((double) tv.tv_usec / 1000000);
}

// One global finish scope

static void initializeKey() {
    pthread_key_create(&selfKey, NULL);
}

void set_current_worker(int wid) {
    pthread_setspecific(selfKey, &workers[wid].id);
}

int hclib_current_worker() {
    return *((int *) pthread_getspecific(selfKey));
}

int hclib_num_workers() {
    return nb_workers;
}

//FWD declaration for pthread_create
void * worker_routine(void * args);

void setup() {
    // Build queues
    not_done = 1;
    pthread_once(&selfKeyInitialized, initializeKey);
    workers = (hclib_worker_state*) malloc(sizeof(hclib_worker_state) * nb_workers);
    for(int i=0; i<nb_workers; i++) {
      workers[i].deque = malloc(sizeof(deque_t));
      workers[i].my_info = malloc(sizeof(infoList_t));
      void * val = NULL;
      dequeInit(workers[i].deque, val);
      infoInit(workers[i].my_info);
    //initialize taskInfo list

      workers[i].current_finish = NULL;
      workers[i].id = i;
    }
    // Start workers
    for(int i=1;i<nb_workers;i++) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&workers[i].tid, &attr, &worker_routine, &workers[i].id);
    }
    set_current_worker(0);
    // allocate root finish
    start_finish();
}

void check_in_finish(finish_t * finish) {
    if(finish) hc_atomic_inc(&(finish->counter));
}

void check_out_finish(finish_t * finish) {
    if(finish) hc_atomic_dec(&(finish->counter));
}

void hclib_init(int argc, char **argv) {
    printf("---------HCLIB_RUNTIME_INFO-----------\n");
    printf(">>> HCLIB_WORKERS\t= %s\n", getenv("HCLIB_WORKERS"));
    printf("----------------------------------------\n");
    nb_workers = (getenv("HCLIB_WORKERS") != NULL) ? atoi(getenv("HCLIB_WORKERS")) : 1;
    setup();
    benchmark_start_time_stats = mysecond();
}

void execute_task(task_t * task) {
    finish_t* current_finish = task->current_finish;
    int wid = hclib_current_worker();
    hclib_worker_state* ws = &workers[wid];
    ws->current_finish = current_finish;
    task->_fp((void *)task->args);
    check_out_finish(current_finish);
    free(task);
}

void spawn(task_t * task) {
    // get current worker
    int wid = hclib_current_worker();
    hclib_worker_state* ws = &workers[wid];
    check_in_finish(ws->current_finish);
    task->current_finish = ws->current_finish;
    // push on worker deq
    dequePush(ws->deque, task);
    ws->total_push++;
}

void hclib_async(generic_frame_ptr fct_ptr, void * arg) {
    task_t * task = malloc(sizeof(*task));
    //assign async id to this task
    int wid = hclib_current_worker();
    workers[wid].async_counter++;
    *task = (task_t){
        ._fp = fct_ptr,
        .args = arg,
        .task_id = workers[wid].async_counter,
    };
    // printf("W%d creates async with id %u\n", wid, workers[wid].async_counter);
    spawn(task);
}

void reset_worker_AC_counter(int numWorkers) {
    workers[0].async_counter = 0;
    printf("W%d AC = %u\n", 0, workers[0].async_counter);
    for(int i=1; i<numWorkers; i++) {
        int workerID = workers[i].id;
        workers[i].async_counter = workers[i-1].async_counter + UINT_MAX / numWorkers;
        printf("W%d AC = %u\n", workerID, workers[i].async_counter);
    }
}

void reset_worker_SC_counters(int numWorkers) {
    for(int i=0; i<numWorkers; i++) {
        workers[i].steal_counter = 0;
    }
}

void hclib_start_tracing() {
//    printf("start tracing\n");
   tracing_enabled = true;
   reset_worker_AC_counter(nb_workers);
   /* Each worker’s AC value set to (workerID * UINT_MAX/numWorkers) */
   reset_worker_SC_counters(nb_workers);
}

void list_aggregation() { 
    //aggregate all the lists based on wc_id
    //create new info lists for aggregation
    infoList_t **temp = (infoList_t**) malloc(nb_workers * sizeof(infoList_t*));
    for (int i=0;i < nb_workers; i++) {
        temp[i] = (infoList_t*) malloc(sizeof(infoList_t*));
        infoInit(temp[i]);
    }
    for (int i=0; i < nb_workers; i++) {
        //traverse ith worker info list and transfer the stolen asyncs to its corresponding creator
        taskInfo_t *cur = workers[i].my_info->head;
        while (cur != NULL) {
            int c_id = cur->wc_id;
            //add this task to c_id's info list
            append_task_info(temp[c_id], cur);
            // printf("appending %u, from W%d to W%d\n", cur->task_id, i, c_id);
            taskInfo_t *t = cur;
            cur = cur->next;
            t->next = NULL;
        }
    }
    // printf("After aggregation\n");
    // for (int i=0; i < nb_workers; i++) {
    //     printf("-----------------------------------------------\n");
    //     display_info_list(temp[i]);
    //     printf("-----------------------------------------------\n");
    // }
    // printf("=====================================================\n");
    //replace initial info lists by aggregated
    for (int i=0; i < nb_workers; i++) {
        workers[i].my_info = temp[i];
        printf("-----------------------------------------------\n");
        display_info_list(temp[i]);
        printf("-----------------------------------------------\n");
    }
    // printf("=====================================================\n");
}

// void cmp(const void *a, const void *b) {
//     const infoList_t *l1 = a;
//     const infoList_t *l2 = b;
//     return l1->
// }

taskInfo_t* sortList(taskInfo_t* start, taskInfo_t* end) {
    if (start == NULL || end == NULL) return NULL;
    if (start == end) {
        return start;
    }
    //recursively sort the two halves
    //determine the middle of linked list
    taskInfo_t *slow, *fast, *prev = NULL;
    slow = fast = start;
    while (fast != NULL && fast->next != NULL) {
        prev = slow;
        slow = slow->next;
        fast = fast->next->next;
    }
    taskInfo_t *head1, *head2;
    if (fast != NULL) {
        prev = slow->next;
        slow->next = NULL;
        // printf("start-> %u slow-> %u\n", start->task_id, slow->task_id);
        head1 = sortList(start, slow);
        head2 = sortList(prev, end);
        // printf("after head1-> %u head2-> %u\n", head1->task_id, head2->task_id);
    }else {
        if (prev != NULL) prev->next = NULL;
        // printf("else start-> %u slow-> %u\n", start->task_id, slow->task_id);
        head1 = sortList(start, prev);
        head2 = sortList(slow, end);
        // printf("else after head1-> %u head2-> %u\n", head1->task_id, head2->task_id);
    }

    //now merge the two sorted halves
    taskInfo_t *cur1, *cur2, *head, *cur, *tail;
    cur1 = head1; cur2 = head2; cur = head = NULL;
    while (cur1 != NULL && cur2 != NULL) {
        if (cur1->task_id <= cur2->task_id) {
            if (head == NULL) {
                head = cur = cur1;
            }else {
                cur->next = cur1;
                cur = cur->next;
            }
            cur1 = cur1->next;
        }else {
            if (head == NULL) {
                head = cur = cur2;
            }else {
                cur->next = cur2;
                cur = cur->next;
            }
            cur2 = cur2->next;
        }
    }
    while (cur1 != NULL) {
        if (head == NULL) {
            head = cur = cur1;
        }else {
            cur->next = cur1;
            cur = cur->next;
        }
        cur1 = cur1->next;
    }
    while (cur2 != NULL) {
        if (head == NULL) {
            head = cur = cur2;
        }else {
            cur->next = cur2;
            cur = cur->next;
        }
        cur2 = cur2->next;
    }
    // printf("after sorting head-> %u \n", head->task_id);
    return head;
}

void list_sorting() {
    //sort all the aggregated info lists by steal counter
    for (int i=0; i < nb_workers; i++) {
        // printf("W%d sorting starts\n", i);
        workers[i].my_info->head = sortList(workers[i].my_info->head, workers[i].my_info->tail);
        printf("W%d list after sorting\n", i);
        display_info_list(workers[i].my_info);
        // int size = sizeof(workers[i].my_info) / sizeof(infoList_t*);
        // qsortList(workers[i].my_info, size, sizeof(infoList_t*), cmp);
    }
}

void hclib_stop_tracing() {
    // // printf("stop tracing\n");
    for (int i=0; i < nb_workers; i++) {
        printf("-------------------------------------------------\n");
        printf("W%d push: %d steals: %lu\n", i, workers[i].total_push, workers[i].steal_counter);
        display_info_list(workers[i].my_info);
        printf("-------------------------------------------------\n");
    }
    if(replay_enabled == false) {
        list_aggregation(nb_workers);
        list_sorting(nb_workers);
        // create_array_to_store_stolen_task(nb_workers);
        replay_enabled = true; 
    }
}

void slave_worker_finishHelper_routine(finish_t* finish) {
   int wid = hclib_current_worker();
   while(finish->counter > 0) {
       task_t* task = dequePop(workers[wid].deque);
       if (!task) {
           // try to steal
           int i = 1;
           while(finish->counter > 0 && i < nb_workers) {
               task = dequeSteal(workers[(wid+i)%(nb_workers)].deque);
                if(task) {
                    //append the task info to its list before executing
                    taskInfo_t *info = (taskInfo_t *) malloc(sizeof(taskInfo_t));
                    info->task_id = task->task_id;
                    info->wc_id = (wid+i)%(nb_workers);
                    info->ws_id = wid;
                    info->steal_counter = workers[wid].steal_counter++;
                    info->next = NULL;
                    append_task_info(workers[wid].my_info, info);
                    workers[wid].total_steals++;	   
                    break;
                }
                i++;
	        }
        }
        if(task) {
            execute_task(task);
        }
    }
}

void start_finish() {
    int wid = hclib_current_worker();
    hclib_worker_state* ws = &workers[wid];
    finish_t * finish = (finish_t*) malloc(sizeof(finish_t));
    finish->parent = ws->current_finish;
    check_in_finish(finish->parent);
    ws->current_finish = finish;
    finish->counter = 0;
}

void end_finish(){ 
    int wid = hclib_current_worker();
    hclib_worker_state* ws = &workers[wid];
    finish_t* current_finish = ws->current_finish;
    if (current_finish->counter > 0) {
        slave_worker_finishHelper_routine(current_finish);
    }
    assert(current_finish->counter == 0);
    check_out_finish(current_finish->parent); // NULL check in check_out_finish
    ws->current_finish = current_finish->parent;
    free(current_finish);
}

void display_info_list(infoList_t *my_list) {
    taskInfo_t *cur = my_list->head;
    while (cur != NULL) {
        printf("%u %d %d %u\n", cur->task_id, cur->wc_id, cur->ws_id, cur->steal_counter);
        cur = cur->next;
    }
}

void hclib_finalize() {
    end_finish();
    not_done = 0;
    int i;
    int tpush=workers[0].total_push, tsteals=workers[0].total_steals;
    for(i=1;i< nb_workers; i++) {
        pthread_join(workers[i].tid, NULL);
        printf("-------------------------------------------------\n");
	tpush+=workers[i].total_push;
	tsteals+=workers[i].total_steals;
    }
    double duration = (mysecond() - benchmark_start_time_stats) * 1000;
    printf("============================ Tabulate Statistics ============================\n");
    printf("time.kernel\ttotalAsync\ttotalSteals\n");
    printf("%.3f\t%d\t%d\n",user_specified_timer,tpush,tsteals);
    printf("=============================================================================\n");
    printf("===== Total Time in %.f msec =====\n", duration);
    printf("===== Test PASSED in 0.0 msec =====\n");
}

void hclib_kernel(generic_frame_ptr fct_ptr, void * arg) {
    double start = mysecond();
    fct_ptr(arg);
    user_specified_timer = (mysecond() - start)*1000;
}

void hclib_finish(generic_frame_ptr fct_ptr, void * arg) {
    start_finish();
    fct_ptr(arg);
    end_finish();
}

void append_task_info(infoList_t *my_info, taskInfo_t *info) {
    if (my_info->head == NULL) {
        // printf("first time W%d my_info\n", hclib_current_worker());
        //list is empty
        my_info->head = info;
        my_info->tail = info;
    } else {
        // printf("W%d my_info\n", hclib_current_worker());
        //append to the tail
        my_info->tail->next = info;
        my_info->tail = info;
    }
    // printf("W%d tail: %u\n", hclib_current_worker(), my_info->tail->task_id);
}

void* worker_routine(void * args) {
    int wid = *((int *) args);
   set_current_worker(wid);
   while(not_done) {
       task_t* task = dequePop(workers[wid].deque);
       if (!task) {
           // try to steal
           int i = 1;
           while (i < nb_workers) {
                task = dequeSteal(workers[(wid+i)%(nb_workers)].deque);
                if(task) {
                    //append the task info to its list before executing
                    taskInfo_t *info = (taskInfo_t *) malloc(sizeof(taskInfo_t));
                    info->task_id = task->task_id;
                    info->wc_id = (wid+i)%(nb_workers);
                    info->ws_id = wid;
                    info->steal_counter = workers[wid].steal_counter++;
                    info->next = NULL;
                    append_task_info(workers[wid].my_info, info);
                    workers[wid].total_steals++;
                    break;
                }
                i++;
	        }
        }
        if(task) {
            execute_task(task);
        }
    }
    return NULL;
}
