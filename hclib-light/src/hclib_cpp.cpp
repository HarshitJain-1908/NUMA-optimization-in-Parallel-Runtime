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

#include "hclib.hpp"

int hclib::current_worker() {
    return hclib_current_worker();
}

int hclib::num_workers() {
    return hclib_num_workers();
}

void hclib::init(int argc, char** argv) {
  hclib_init(argc, argv);
}

void hclib::finalize() {
  hclib_finalize();
}

void hclib::start_tracing() {
  hclib_start_tracing();
}

void hclib::stop_tracing() {
  hclib_stop_tracing();
}
