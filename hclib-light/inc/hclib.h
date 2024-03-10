#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef void (*generic_frame_ptr)(void*);

struct finish_t;

typedef struct task_t {
    //adding metadata for task
    unsigned int task_id;
    void *args;
    generic_frame_ptr _fp;
    struct finish_t* current_finish;
} task_t;

typedef struct taskInfo_t {
    unsigned int task_id;
    int wc_id, ws_id; //wc_id -> id of worker creating this task, ws_id -> id of worker executing this task
    unsigned int steal_counter;
    struct taskInfo_t* next;
} taskInfo_t;

typedef struct {
    volatile taskInfo_t *head, *tail;
} infoList_t;

/**
 * @brief Spawn a new task asynchronously.
 * @param[in] fct_ptr           The function to execute
 * @param[in] arg               Argument to the async
 */
void hclib_async(generic_frame_ptr fct_ptr, void * arg);
void hclib_finish(generic_frame_ptr fct_ptr, void * arg);
void hclib_kernel(generic_frame_ptr fct_ptr, void * arg);
int hclib_current_worker();
void hclib_start_tracing();
void hclib_stop_tracing();
void start_finish();
void end_finish();
int hclib_num_workers();
void hclib_init(int argc, char **argv);
void hclib_finalize();
#ifdef __cplusplus
}
#endif
