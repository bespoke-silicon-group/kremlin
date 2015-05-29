# kremlin: like gprof but for parallelization

 Kremlin is a tool that, given a serial program, tells you which regions to parallelize.

To create Kremlin we developed a novel dynamic analysis, _hierarchical critical path analysis_, to detect parallelism across nested regions of the program, which connects to a _parallelism planner_ which evaluates many potential parallelizations to figure out the best way for the user to parallelize the target program. 