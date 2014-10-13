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

- libbfd: for dagviz to read binary file.
  Ubuntu 14.04: binutils-dev package

Optional:

- libunwind: for getting call chains
  Ubuntu 14.04: libunwind8-dev package