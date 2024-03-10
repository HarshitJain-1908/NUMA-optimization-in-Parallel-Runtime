This is an optimization in the HCLib parallel runtime by improving its support for locality over modern multicore processors.
Here locality of iterative algorithms has been improved by using trace/replay to co-locate the data & its computation to
reduce the execution time.

Benchmark(s):
* iterative averaging
