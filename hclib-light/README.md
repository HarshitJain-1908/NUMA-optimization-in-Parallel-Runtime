HClib-Light
=============================================

HClib is a task-based parallel programming model that supports the finish-async,
parallel-for, async_at_hpt, and future-promise parallel programming patterns through both C
and C++ APIs.

HClib-Light (this version) is a lightweight implementation of the original HCLIB that
only supports the async-finish APIs. It is a minimalistic implementation derived specially
for the the CSE513 course at IIIT-Delhi. 

YOU ARE NOT ALLOWED TO OPEN-SOURCE THIS IMPLEMENTATION OF HCLIB

Installation
---------------------------------------------

HClib follows your standard bootstrap, configure, and make installation
procedure. An install.sh script is provided for your convenience that will
build and install HClib. Simply run the script to install:

    ./install.sh
By default, HClib will be installed to `$PWD/hclib-install`.

You will need to set the `HCLIB_ROOT` environment variable to point to your
HClib installation directory. You can automatically set this variable after
installation by sourcing the `hclib_setup_env.sh` script. For example, assuming
HClib was installed inside `/home/vivek/hclib`:

source /home/vivek/hclib/hclib-install/bin/hclib_setup_env.sh

Running HClib Programs
---------------------------------------------
cd hclib/test
make
HCLIB_WORKERS=4 ./fib

The above invocation would create four HClib workers (pthread) that would
use work-stealing algorithm for load-balancing of async tasks

Dependencies
---------------------------------------------

* automake
* gcc >= 4.8.4, or clang >= 3.5
  (must support -std=c11 and -std=c++11)
