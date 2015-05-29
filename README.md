# kremlin: like gprof but for parallelization

Kremlin is a tool that, given a serial program, tells you which regions to
parallelize.

To create Kremlin we developed a novel dynamic analysis, _hierarchical critical
path analysis_, to detect parallelism across nested regions of the program,
which connects to a _parallelism planner_ which evaluates many potential
parallelizations to figure out the best way for the user to parallelize the
target program. 

## Example Usage

The following shows you the basic flow of using Kremlin to create a
parallelization plan for your program.

```bash
make CC=kremlin-gcc
./kmeans data.large.txt
kremlin --planner=openmp

PlannerType = OpenMP
Target : NumCore = 4, Overhead = 0,  Cache = 0 MB, BW= 0 MB/s
Speedup: 4.00
Serial  : 182
Parallel: 45

[ 0] TimeRed(4)=62.23%, TimeRed(Ideal)=75.27%, Cov=82.97%, SelfP=10.79, DOALL
  LOOP     main.c [  19 -   23]:        main

[ 1] TimeRed(4)=12.77%, TimeRed(Ideal)=15.93%, Cov=17.03%, SelfP=15.50, DOALL
  LOOP     main.c [  11 -   13]:        main
```

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

## Using Kremlin

The basic flow for using Kremlin requires three steps:

1. Compile your program with `kremlin-gcc` or `kremlin-g++`. These programs are
   drop-in replacements for `gcc` and `g++`, respectively. 
1. Run your program as you normally would.
1. Run the parallelism planner by invoking the `kremlin` program within the
   same directory that you build/ran your program.
   By default, Kremlin gives you a plan based on OpenMP.
