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

// pthread_mutex_t lock;

hclib_worker_state* workers;
int * worker_id;
int nb_workers;
int not_done;
static double user_specified_timer = 0;
static double benchmark_start_time_stats = 0;

//volatile(??)
volatile static bool tracing_enabled = false;
volatile static bool replay_enabled = false;

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



// int get_nb_cpus() {
//     int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
//     return numCPU;
// }

// void bind_thread_with_mask(int *mask, int lg) {
//     cpu_set_t cpuset;
//     if (mask != NULL) {
//         CPU_ZERO(&cpuset);

//         /* Copy the mask from the int array to the cpuset */
//         int i;
//         for (i = 0; i < lg; i++) {
//             CPU_SET(mask[i], &cpuset);
//         }

//         /* Set affinity */
//         int res = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
//         if (res != 0) {
//             printf("ERROR: ");
//             if (errno == ESRCH) {
//                 HASSERT("THREADBINDING ERROR: ESRCH: Process not found!\n");
//             }
//             if (errno == EINVAL) {
//                 HASSERT("THREADBINDING ERROR: EINVAL: CPU mask does not contain any actual physical processor\n");
//             }
//             if (errno == EFAULT) {
//                 HASSERT("THREADBINDING ERROR: EFAULT: memory address was invalid\n");
//             }
//             if (errno == EPERM) {
//                 HASSERT("THREADBINDING ERROR: EPERM: process does not have appropriate privileges\n");
//             }
//         }
//     }
// }

// /* Bind threads in a round-robin fashion */
// void bind_thread_rr(int worker_id) {
//     /*bind worker_id to cpu_id round-robin fashion*/
//     int nbCPU = get_nb_cpus();
//     int mask = worker_id % nbCPU;
//     //printf("HCLIB: INFO -- Binding worker %d to cpu_id %d\n", worker_id, mask);
//     bind_thread_with_mask(&mask, 1);
// }

// /* Bind threads according to bind map */
// void bind_thread_map(int worker_id, int *bind_map, int bind_map_size) {
//     int mask = bind_map[worker_id % bind_map_size];
//     //printf("HCLIB: INFO -- Binding worker %d to cpu_id %d\n", worker_id, mask);
//     bind_thread_with_mask(&mask, 1);
// }

// /** Thread binding api to bind a worker thread using a particular binding strategy **/
// void bind_thread(int worker_id, int *bind_map, int bind_map_size) {
//     if (bind_map_size == 0) {
//         /* Round robin binding */
//         bind_thread_rr(worker_id);
//     } else if (bind_map_size > 0 && bind_map != NULL) {
//         bind_thread_map(worker_id, bind_map, bind_map_size);
//     } else {
//         fprintf(stderr, "ERROR: unknown thread binding strategy\n");
//         HASSERT(0);
//     }
// }


//FWD declaration for pthread_create
void * worker_routine(void * args);

void setup() {
    // Build queues
    // pthread_mutex_init(&lock, NULL);
    not_done = 1;
    pthread_once(&selfKeyInitialized, initializeKey);
    workers = (hclib_worker_state*) malloc(sizeof(hclib_worker_state) * nb_workers);
    for(int i=0; i<nb_workers; i++) {
      workers[i].deque = malloc(sizeof(deque_t));
      workers[i].my_info = malloc(sizeof(infoList_t));
    //   workers[i].available_traced_steals_tasks = 
      void * val = NULL;
      dequeInit(workers[i].deque, val);
      infoInit(workers[i].my_info);
    //initialize taskInfo list
    //   pthread_mutex_init(&workers[i].lock, NULL);
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
    // printf("am here by W%d\n", hclib_current_worker());//task->task_id, hclib_current_worker());
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
    // if (replay_enabled) {
    //     printf("W%d spawned task %u replay:%d oops: %d\n", wid, task->task_id, replay_enabled, workers[wid].my_info == NULL);
    //     if (workers[wid].my_info->tail != NULL) printf("W%d tail:%d id:%d\n", wid, workers[wid].my_info->tail != NULL, task->task_id == workers[wid].my_info->tail->task_id);
    // }
    if (replay_enabled && workers[wid].my_info->tail != NULL && task->task_id == workers[wid].my_info->tail->task_id) {
        //send this task its thief during trace iteration
        int ws_id = workers[wid].my_info->tail->ws_id;
        int sc = workers[wid].my_info->tail->steal_counter;
        // printf("--->W%d at sc=%d task info task_id:%u\n", ws_id, sc, task->task_id);
        // pthread_mutex_lock(&workers[ws_id].lock);


        //push in the deque
        // hclib_worker_state* ws1 = &workers[ws_id];
        // dequePush(ws1->deque, task);

        // ws1->deque->data[sc] = task;

        workers[ws_id].traced_steals_deque[sc] = task;
        // pthread_mutex_unlock(&workers[ws_id].lock);
        
        // printf("Store task_id:%u @ W%d at sc=%d\n", workers[ws_id].traced_steals_deque[sc]->task_id, ws_id, sc);

        workers[ws_id].available_traced_steals_tasks[sc] = true;
        // printf("Store task_id:%u @ W%d at sc=%d\n", ((task_t*)(ws1->deque->data[sc]))->task_id, ws_id, sc);
        
        //-----------------------------------
        //do we need lock here??
        // printf("making available true for W%d\n", ws_id);
        // if (!workers[ws_id].available_traced_steals_tasks[sc]) 
        
        //-----------------------------------

        workers[wid].my_info->tail = workers[wid].my_info->tail->next;
    } else {
        // push on worker deq
        dequePush(ws->deque, task);
    }
    ws->total_push++;
}

void hclib_async(generic_frame_ptr fct_ptr, void * arg) {
    task_t * task = malloc(sizeof(*task));
    int wid = hclib_current_worker();
    workers[wid].async_counter++;
    *task = (task_t){
        ._fp = fct_ptr,
        .args = arg,
        .task_id = workers[wid].async_counter, //assign async id to this task
    };
    // printf("W%d creates async with id %u\n", wid, workers[wid].async_counter);
    spawn(task);
}

void reset_worker_AC_counter(int numWorkers) {
    workers[0].async_counter = 0;
    // printf("W%d AC = %u\n", 0, workers[0].async_counter);
    for(int i=1; i<numWorkers; i++) {
        int workerID = workers[i].id;
        workers[i].async_counter = workers[i-1].async_counter + UINT_MAX / numWorkers;
        // workers[i].deque->head = workers[i].deque->tail = 0;
        // printf("W%d AC = %u\n", workerID, workers[i].async_counter);
    }
}

void reset_worker_SC_counters(int numWorkers) {
    for(int i=0; i<numWorkers; i++) {
        workers[i].steal_counter = 0;
    }
}

// void reset_worker_traced_steals(int numWorkers) {
//     for(int i=0; i<numWorkers; i++) {
//         workers[i].available_traced_steals_tasks = false;
//     }
//     printf("making it false\n");
// }

void hclib_start_tracing() {
//    printf("start tracing\n");
   tracing_enabled = true;
   reset_worker_AC_counter(nb_workers);
   reset_worker_SC_counters(nb_workers);
//    reset_worker_traced_steals(nb_workers);
}

void list_aggregation() { 
    //aggregate all the lists based on wc_id
    //create new info lists for aggregation
    infoList_t **temp = (infoList_t**) malloc(nb_workers * sizeof(infoList_t*));
    for (int i=0;i<nb_workers; i++) {
        temp[i] = (infoList_t*) malloc(sizeof(infoList_t*));
        infoInit(temp[i]);
    }
    for (int i=0; i<nb_workers; i++) {
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
    // for (int i=0; i<nb_workers; i++) {
    //     printf("-----------------------------------------------\n");
    //     display_info_list(temp[i]);
    //     printf("-----------------------------------------------\n");
    // }
    // printf("=====================================================\n");
    //replace initial info lists by aggregated
    for (int i=0; i<nb_workers; i++) {
        workers[i].my_info = temp[i];
        // printf("-----------------------------------------------\n");
        // display_info_list(temp[i]);
        // printf("-----------------------------------------------\n");
    }
    // printf("=====================================================\n");
}

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
    for (int i=0; i<nb_workers; i++) {
        // printf("W%d sorting starts\n", i);
        workers[i].my_info->head = sortList(workers[i].my_info->head, workers[i].my_info->tail);
        // printf("W%d list after sorting\n", i);
        // display_info_list(workers[i].my_info);
        // int size = sizeof(workers[i].my_info) / sizeof(infoList_t*);
        // qsortList(workers[i].my_info, size, sizeof(infoList_t*), cmp);
    }
}

void create_array_to_store_stolen_task() {
    for (int i=0; i<nb_workers; i++) {
        int size = workers[i].steal_counter;
        workers[i].traced_steals_deque = (task_t**) malloc(size*(sizeof(task_t*)));
        // // for (int j=0; j<size; j++) {
        // //     // task_t* t = (task_t*) malloc(sizeof(task_t*));
        // //     // t->task_id = 0;
        // //     workers[i].traced_steals_deque[j] = NULL;
        // // }
        workers[i].size = size;
        // // for (int j=0; j < size; j++) {
        // //     workers[i].traced_steals_deque[j]->data = ;
        // // }
        // workers[i].deque->head = workers[i].deque->tail = 0;
        // for (int j=0; j<INIT_DEQUE_CAPACITY; j++) {
        //     workers[i].deque->data[j] = NULL;
        // }
    }
}

void hclib_stop_tracing() {
    // // printf("stop tracing\n");
    
    // for (int i=0; i<nb_workers; i++) {
    //     printf("-------------------------------------------------\n");
    //     printf("W%d push: %d steals: %lu\n", i, workers[i].total_push, workers[i].steal_counter);
    //     display_info_list(workers[i].my_info);
    //     printf("-------------------------------------------------\n");
    // }
    if(replay_enabled == false) {
        list_aggregation();
        list_sorting();
        create_array_to_store_stolen_task();
        replay_enabled = true;
    }
    for (int i=0; i<nb_workers; i++) {
        workers[i].my_info->tail = workers[i].my_info->head;
        // if (workers[i].my_info->tail != NULL) printf("W%d tail points to task %u\n", i, workers[i].my_info->tail->task_id);  
        // workers[i].deque->tail = workers[i].deque->head;
        // deq->data = (volatile void **) malloc(sizeof(void*)*INIT_DEQUE_CAPACITY);
        // pthread_mutex_lock(&lock);
        // while(k < INIT_DEQUE_CAPACITY) {
        //     workers[i].deque->data[k] = NULL;
        //     k++;
        // }
        
        // printf("W%d --> head:%d tail:%d\n", i, workers[i].deque->head, workers[i].deque->tail);
        // pthread_mutex_unlock(&lock);
        // dequeInit(workers[i].deque, val);
        // workers[i].deque->head = workers[i].deque->tail = 0;
        int size = workers[i].steal_counter;
        if (size > 0) {
            workers[i].available_traced_steals_tasks = (volatile bool*) malloc(workers[i].steal_counter*sizeof(volatile bool));
            for (int j=0; j<size; j++) {
                workers[i].available_traced_steals_tasks[j] = false;
            }
        }
        // for (int j=0; j<INIT_DEQUE_CAPACITY; j++) {
        //     if (workers[i].deque->data[j] != NULL) workers[i].deque->data[j] = NULL;
        // }
    }
}

void slave_worker_finishHelper_routine(finish_t* finish) {
   int wid = hclib_current_worker();
   while(finish->counter > 0) {
        // printf("slave before popping W%d head:%d tail:%d\n", wid, workers[wid].deque->head, workers[wid].deque->tail);
        task_t* task = dequePop(workers[wid].deque);
        if(task) {
            // printf("slave W%d got task %u head:%d tail:%d\n", wid, task->task_id, workers[wid].deque->head, workers[wid].deque->tail);
            execute_task(task);
        }
        if (!task) {
            if (replay_enabled) {
                // printf("checking task by W%d from sc=%u status: %d\n", wid, workers[wid].steal_counter, workers[wid].available_traced_steals_tasks);
                // printf("match %u == %d\n", workers[wid].steal_counter, workers[wid].size);
                if (workers[wid].steal_counter == workers[wid].size) continue;
                while (!workers[wid].available_traced_steals_tasks[workers[wid].steal_counter]);
                    // pthread_mutex_lock(&workers[wid].lock);
                    // pthread_mutex_unlock(&workers[wid].lock);
                    // printf("checking task by W%d from sc=%u\n", wid, workers[wid].steal_counter);
                    // while (workers[wid].traced_steals_deque[workers[wid].steal_counter] != NULL && workers[wid].traced_steals_deque[workers[wid].steal_counter]->task_id <= 0);
                // printf("slave hello\n");
                task = workers[wid].traced_steals_deque[workers[wid].steal_counter];
                    // 
                    // while (workers[wid].deque->data[workers[wid].steal_counter] == NULL);
                // printf("slave hello again\n");
                    // while (((task_t*)(workers[wid].deque->data[workers[wid].steal_counter]))->task_id <= 0);
                    // printf("bye\n");
                // task = (task_t*)(workers[wid].deque->data[workers[wid].steal_counter]);
                if (task && task->task_id > 0) {
                    // printf("Slave W%d set for pickup from SC:%u\n", wid, workers[wid].steal_counter);
                    // printf("slave executing task %u by W%d sc=%u\n", task->task_id, wid, workers[wid].steal_counter);
                    workers[wid].steal_counter++;
                    workers[wid].total_steals++;
                    // if (workers[wid].steal_counter == workers[wid].size) {
                    //     workers[wid].available_traced_steals_tasks[workers[wid].steal_counter] = false;
                    //     // printf("in slave making it false for W%d %d ?? %d\n", wid, workers[wid].steal_counter, workers[wid].size);
                    // }
                    execute_task(task);
                    // exec(task);
                    // workers[wid].traced_steals_deque[workers[wid].steal_counter-1] = NULL;
                    // task = workers[wid].traced_steals_deque[workers[wid].steal_counter-1];
                    // task = (task_t*)(workers[wid].deque->data[workers[wid].steal_counter]);
                    // if (workers[wid].traced_steals_deque[workers[wid].steal_counter-1] != NULL && workers[wid].traced_steals_deque[workers[wid].steal_counter-1]->task_id > 0) {
                    //     printf("executing again\n");
                    //     // execute_task(task);
                    // }
                    // workers[wid].steal_counter++;
                    //free it
                    // workers[wid].traced_steals_deque[workers[wid].steal_counter-1] = NULL;
                }
            } else {
                // try to steal
                int i = 1;
                while (i<nb_workers) {
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
                if(task) {
                    execute_task(task);
                }
            }
        }
        // if(task) {
        //     execute_task(task);
        // }
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
    // pthread_mutex_destroy(&lock);
    int i;
    int tpush=workers[0].total_push, tsteals=workers[0].total_steals;
    for(i=1;i< nb_workers; i++) {
        pthread_join(workers[i].tid, NULL);
        tpush+=workers[i].total_push;
        tsteals+=workers[i].total_steals;
        // pthread_mutex_destroy(&workers[i].lock);
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
        if(task) {
            // printf("W%d got task %u\n", wid, task->task_id);
            execute_task(task);
        }
        if (!task) {
            if (replay_enabled) {
                // printf("match %u == %d\n", workers[wid].steal_counter, workers[wid].size);
                // printf("hey from W%d\n", wid);
                // printf("hey from W%d sc=%u status:%d\n", wid, workers[wid].steal_counter, workers[wid].available_traced_steals_tasks[workers[wid].steal_counter]);
                // printf("hey from W%d sc:%u\n", wid, workers[wid].steal_counter);
                if (workers[wid].steal_counter == workers[wid].size) continue;
                // printf("hey from W%d sc:%u\n", wid, workers[wid].steal_counter);
                while (!workers[wid].available_traced_steals_tasks[workers[wid].steal_counter]);
                // printf("W%d set for pickup from SC:%u\n", wid, workers[wid].steal_counter);
                // while (workers[wid].traced_steals_deque[workers[wid].steal_counter] == NULL || workers[wid].traced_steals_deque[workers[wid].steal_counter]->task_id <= 0);
                // printf("hello\n");
                task = workers[wid].traced_steals_deque[workers[wid].steal_counter];
                // printf("hello again\n");
                // while (workers[wid].deque->data[workers[wid].steal_counter] == NULL || ((task_t*)(workers[wid].deque->data[workers[wid].steal_counter]))->task_id <= 0);
                // task = (task_t*)(workers[wid].deque->data[workers[wid].steal_counter]);
                if (task && task->task_id > 0) {
                    // printf("W%d take out task %u\n", wid, task->task_id);
                    // if (wid == 1) printf("executing task %u by W%d from sc=%u\n", task->task_id, wid, workers[wid].steal_counter);
                    // printf("oof\n");
                    workers[wid].steal_counter++;
                    workers[wid].total_steals++;
                    // if (workers[wid].steal_counter == workers[wid].size) {
                    //     workers[wid].available_traced_steals_tasks[workers[wid].steal_counter] = false;
                    //     printf("making it false for W%d %d ?? %d\n", wid, workers[wid].steal_counter, workers[wid].size);
                    // }
                    // exec(task);
                    
                    execute_task(task);
                    // printf("executing task %u by W%d from sc=%u %u\n", task->task_id, wid, workers[wid].steal_counter-1, workers[wid].steal_counter);
                    // free(workers[wid].traced_steals_deque[workers[wid].steal_counter-1]);
                    //= (void*)NULL;
                    // task = workers[wid].traced_steals_deque[workers[wid].steal_counter-1];
                    // workers[wid].deque->data[workers[wid].steal_counter-1] = NULL;
                    // task = (task_t*)(workers[wid].deque->data[workers[wid].steal_counter]);
                    // if (workers[wid].traced_steals_deque[workers[wid].steal_counter-1] != NULL && workers[wid].traced_steals_deque[workers[wid].steal_counter-1]->task_id > 0) {
                    //     printf("executing again\n");
                    //     // execute_task(task);
                    // }
                    // workers[wid].steal_counter++;
                    //free it
                    // workers[wid].traced_steals_deque[workers[wid].steal_counter-1] = NULL;
                }
            } else {
                // try to steal
                int i = 1;
                while (i<nb_workers) {
                    task = dequeSteal(workers[(wid+i)%(nb_workers)].deque);
                    if(task) {
                        //append the task info to its list before executing
                        taskInfo_t *info = (taskInfo_t *) malloc(sizeof(taskInfo_t));
                        info->task_id = task->task_id;
                        info->wc_id = (wid+i)%(nb_workers);
                        info->ws_id = wid;
                        info->steal_counter = workers[wid].steal_counter++;
                        info->next = NULL;
                        workers[wid].total_steals++;
                        append_task_info(workers[wid].my_info, info);
                        break;
                    }
                    i++;
                }
                if(task) {
                    execute_task(task);
                }
            }
        }
        // if(task) {
        //     execute_task(task);
        // }
    }
    return NULL;
}
