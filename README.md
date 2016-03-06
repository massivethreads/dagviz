dagviz
======

Visualizer for computation DAG of task parallel executions

Example:
  make
  ./dagviz data/fib4.dag

Prerequisites
======

You need to have:

- libgtk: otherwise the compilation will fail with missing gtk/gtk.h
  Ubuntu 14.04: libgtk-3-dev package

- libdr: DAG Recorder which resides in MassiveThreads (github.com/massivethreads/massivethreads)
  Ubuntu 14.04: to clone git repo and build from source

Optional:

- libunwind: for getting call chains
  Ubuntu 14.04: libunwind8-dev package

- libbfd: for dagviz to read binary file.
  Ubuntu 14.04: binutils-dev package
