This is an optimization in the HCLib parallel runtime by improving its support for locality over modern multicore processors.
Here locality of iterative algorithms has been improved by using trace/replay to co-locate the data & its computation to
reduce the execution time.

<h2> Benchmark(s): </h2>

* iterative averaging

Run the makefile with relevant compile time flags:
* -DUSE_TRACE_AND_REPLAY for enabling the trace and replay optimization
* -DVERIFY to verify the parallel version of iterative averaging with the sequential version

<h2> Execution </h2>
    HCLIB_WORKERS=4 taskset --cpu-list 0,1,4,5 numactl -i all ./trace_and_replay_iterative_averaging
<br></br>
Note:

  * taskset binds the threads to specific cores to respect the locality during the complete execution of the program
  * numactl -i all is used for interleaved memory allocation across all the NUMA nodes on the underlying processor.
