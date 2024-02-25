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

#if defined __x86_64 || __i686__

#define HC_CACHE_LINE 64


static __inline__ int hc_atomic_inc(volatile int *ptr) {
    unsigned char c;
    __asm__ __volatile__(

            "lock       ;\n"
            "incl %0; sete %1"
            : "+m" (*(ptr)), "=qm" (c)
              : : "memory"
    );
    return c!= 0;
}

/* return 1 if the *ptr becomes 0 after decremented, otherwise return 0
 */
static __inline__ int hc_atomic_dec(volatile int *ptr) {
    unsigned char rt;
    __asm__ __volatile__(
            "lock;\n"
            "decl %0; sete %1"
            : "+m" (*(ptr)), "=qm" (rt)
              : : "memory"
    );
    return rt != 0;
}

static __inline__ void hc_mfence() {
        __asm__ __volatile__("mfence":: : "memory");
}

/*
 * if (*ptr == ag) { *ptr = x, return 1 }
 * else return 0;
 */
static __inline__ int hc_cas(volatile int *ptr, int ag, int x) {
        int tmp;
        __asm__ __volatile__("lock;\n"
                             "cmpxchgl %1,%3"
                             : "=a" (tmp) /* %0 EAX, return value */
                             : "r"(x), /* %1 reg, new value */
                               "0" (ag), /* %2 EAX, compare value */
                               "m" (*(ptr)) /* %3 mem, destination operand */
                             : "memory" /*, "cc" content changed, memory and cond register */
                );
        return tmp == ag;
}

#endif /* __x86_64 */

