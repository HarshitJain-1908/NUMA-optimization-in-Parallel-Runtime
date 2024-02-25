#include <iostream>
// Required for lambda as std::function
#include <list>
#include <functional>
#include "hclib.h"

namespace hclib {
/*
 * The C API to the HC runtime defines a task at its simplest as a function
 * pointer paired with a void* pointing to some user data. This file adds a C++
 * wrapper over that API by passing the C API a lambda-caller function and a
 * pointer to the lambda stored on the heap, which are then called.
 *
 * This does add more overheads in the C++ version (i.e. memory allocations).
 * TODO optimize that overhead.
 */

/* raw function pointer for calling lambdas */
template<typename T>
void lambda_wrapper(void *arg) {
    T *lambda = static_cast<T*>(arg);
    (*lambda)(); // !!! May cause a worker-swap !!!
    delete lambda;
}

template <typename T>
inline void async(T &&lambda) {
    typedef typename std::remove_reference<T>::type U;
    hclib_async(lambda_wrapper<U>, new U(lambda));
}

template <typename T>
inline void finish(T &&lambda) {
    typedef typename std::remove_reference<T>::type U;
    hclib_finish(lambda_wrapper<U>, new U(lambda));
}

template <typename T>
inline void kernel(T &&lambda) {
    typedef typename std::remove_reference<T>::type U;
    hclib_kernel(lambda_wrapper<U>, new U(lambda));
}

void init(int argc, char ** argv);
void finalize();
int current_worker();
int num_workers();
void start_tracing();
void stop_tracing();
}

