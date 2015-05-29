# kremlin: like gprof but for parallelization

Kremlin is a tool that, given a serial program, tells you which regions to
parallelize.

To create Kremlin we developed a novel dynamic analysis, _hierarchical critical
path analysis_, to detect parallelism across nested regions of the program,
which connects to a _parallelism planner_ which evaluates many potential
parallelizations to figure out the best way for the user to parallelize the
target program. 

## Getting Started

To get started clone the kremlin repository and simply run `make` inside the
kremlin directory.
This will take some time as it has to compile LLVM.

After compilation is complete, update your shell's `PATH` to point to Kremlin's
bin directory.
The exact directory to use will have been printed after successfully
compilation.

To ensure you have the proper setup, run the Kremlin test suite by going into
the `kremlin/test` directory and running the `scons` command.
This will take a few minutes the first time you run it; subsequent runs should
be significantly faster.
kremlin will print out a message if any of the tests failed; if you don't see
this message then kremlin has been successfully installed.
