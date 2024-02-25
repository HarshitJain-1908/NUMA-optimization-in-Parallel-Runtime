#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef void (*generic_frame_ptr)(void*);

struct finish_t;

typedef struct task_t {
    void *args;
    generic_frame_ptr _fp;
    struct finish_t* current_finish;
} task_t;

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
