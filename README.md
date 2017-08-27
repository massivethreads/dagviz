DAGViz
======

A DAG visualization and analysis tool for task parallel applications
based on the computation DAG traces captured from their executions.

DAGViz's GUI was first implemented in C language with the GTK+ toolkit,
but then it is changed to Qt5 framework with its Python bindings PyQt5.
So there are currently two versions: *dagviz-gtk* and *dagviz-pyqt*, and the
default one *dagviz* is symlinked to the newer *dagviz-pyqt*.
Qt-based version is easier to be installed and used on other platforms
(e.g., macOS, Windows) rather than the Ubuntu Linux on which it is
originally developed.

![DAGViz GUI](./gui/dagviz_gui.png)

Besides the dependencies on the GUI toolkits, DAGViz also depends on MassiveThreads
for reading the format of the DAG trace files which are recorded by DAG Recorder -
a tracing module included in MassiveThreads.

GTK-based version and Qt-based version can be built separately and independently.
The newer Qt-based version is recommended.
More detailed installation instructions are provided below.


DAGViz-GTK
-----

DAGViz-GTK is written totally in C language with the GTK+ toolkit.

### Dependencies

1. MassiveThreads: https://github.com/massivethreads/massivethreads
2. GTK+ 3: https://www.gtk.org/download (libgtk-3-dev package on Ubuntu)


There are other optional packages for reading sampling-based traces
if necessary:

- libunwind: for getting call chains
  Ubuntu 14.04: libunwind8-dev package

- libbfd: for dagviz to read binary file.
  Ubuntu 14.04: binutils-dev package

### Quick start

```bash
$ git clone git@github.com:zanton/dagviz.git
$ cd dagviz
$ make gtk
$ ./dagviz-gtk
```


DAGViz-PyQt
-----

DAGViz-PyQt is built based on the Qt5 framework,
the GUI part is written in Python language with Qt5's Python bindings PyQt5,
the visualization part which renders DAG on viewport/canvas is written in C++ (Qt5)
in order to better communicate with
the compute part which is maintained in C for the purpose of better performance and 
memory management.

The Python part and C++ part are connected by the SIP tool which is created by the
same company of PyQt5.

### Dependencies
1. MassiveThreads: https://github.com/massivethreads/massivethreads
2. Qt5 v5.9.1: https://www.qt.io/download/
3. SIP 4.19.3: https://www.riverbankcomputing.com/software/sip/download
4. PyQt5 v5.9: https://www.riverbankcomputing.com/software/pyqt/download5
5. Python 3 (python3)

### Quick start

```bash
$ git clone git@github.com:zanton/dagviz.git
$ cd dagviz
$ make qt
$ ./dagviz-pyqt
or
$ make dagviz
$ ./dagviz
```

### Quick troubleshootings

- sometimes Python.h is not found, try adding its path to appropriate environment variables:

  ```bash
  $ export C_INCLUDE_PATH=/usr/include/python3.5:$C_INCLUDE_PATH
  $ export CPLUS_INCLUDE_PATH=/usr/include/python3.5:$CPLUS_INCLUDE_PATH
  ```

- ensure Qt5's *qmake* is included in system's path
  ```bash
  QTDIR=$HOME/local/Qt5.9.1/5.9.1/gcc_64
  PATH=$QTDIR/bin:$PATH
  ```

- sip/configure.py: ensure PyQt5's SIP include directory and Qt5's include directory are included in system's include path,
  otherwise change to correct paths in sip/configure.py, e.g.,
  ```bash
  sip_inc_dir = "$HOME/local/share/sip/PyQt5"
  qt_inc_dir = "$HOME/local/Qt5.9.1/5.9.1/gcc_64/include"
  ```

- ensure PyQt5 modules are included in system's Python path, e.g.,
  ```bash
  $ export PYTHONPATH=$HOME/local/lib/python3.5/dist-packages:$PYTHONPATH
  ```

- On Ubuntu 16.04, some necessary Python and OpenGL packages are:
  ```bash
  $ sudo apt-get install libpython3.5-dev mesa-common-dev libglu1-mesa-dev freeglut3-dev
  ```

### More detailed notes of installing DAGViz-PyQt's dependencies from sources
------

##### MassiveThreads installation:

- given that MassiveThreads is installed as following

  ```bash
  $ git clone git@github.com:massivethreads/massivethreads.git
  $ cd massivethreads
  $ ./configure --prefix=$HOME/local CFLAGS="-Wall -O3"
  $ make
  $ make install
  ```

- add its paths to environment variables:

  ```bash
  $ export PATH=$HOME/local/bin:$PATH:
  $ export C_INCLUDE_PATH=$HOME/local/include:$C_INCLUDE_PATH
  $ export CPLUS_INCLUDE_PATH=$HOME/local/include$CPLUS_INCLUDE_PATH
  $ export LIBRARY_PATH=$HOME/local/lib:$LIBRARY_PATH
  $ export LD_LIBRARY_PATH=$HOME/local/lib:$LD_LIBRARY_PATH
  ```

##### Qt5 installation:

- given that Qt5's installation directory is following

  ```bash
  $HOME/local/Qt5.9.1
  ```

- add Qt5's directory and *qmake*'s path to environment variables

  ```bash
  QTDIR=$HOME/local/Qt5.9.1/5.9.1/gcc_64
  PATH=$QTDIR/bin:$PATH
  ```

##### SIP installation:

- SIP can be installed with following procedure

  ```bash
  $ wget https://sourceforge.net/projects/pyqt/files/sip/sip-4.19.3/sip-4.19.3.tar.gz
  $ tar xf sip-4.19.3.tar.gz
  $ cd sip-4.19.3
  $ python3 configure.py --sysroot=$HOME/local --configuration myconfig.txt
  $ make
  $ make install
  ```

- and the configuration file myconfig.txt has following contents

  ```python
  # The target Python installation.
  py_platform = linux
  py_inc_dir = /usr/include/python%(py_major).%(py_minor)

  # Where SIP will be installed.
  sip_bin_dir = %(sysroot)/bin
  sip_inc_dir = %(sysroot)/include
  sip_module_dir = %(sysroot)/lib/python%(py_major)/dist-packages
  sip_sip_dir = %(sysroot)/share/sip
  ```

- ensure SIP module is included in system's Python path, e.g.,

  ```bash
  $ export PYTHONPATH=$HOME/local/lib/python3/dist-packages:$PYTHONPATH
  ```


##### Python 3:

- sometimes Python.h is not found, try adding its path to appropriate environment variables:

  ```bash
  $ export C_INCLUDE_PATH=/usr/include/python3.5:$C_INCLUDE_PATH
  $ export CPLUS_INCLUDE_PATH=/usr/include/python3.5:$CPLUS_INCLUDE_PATH
  ```

##### PyQt5 installation:

- PyQt5 can be installed with following procedure

  ```bash
  $ wget https://sourceforge.net/projects/pyqt/files/PyQt5/PyQt-5.9/PyQt5_gpl-5.9.tar.gz
  $ tar xf PyQt5_gpl-5.9.tar.gz
  $ cd PyQt5_gpl-5.9
  $ python3 configure.py --sysroot=$HOME/local
  $ make -j
  $ make install
  ```

- sometimes Python.h cannot be found despite *C_INCLUDE_PATH* and *CPLUS_INCLUDE_PATH*
set above, use a configuration file to indicate the path to Python.h

  ```bash
  $ wget https://sourceforge.net/projects/pyqt/files/PyQt5/PyQt-5.9/PyQt5_gpl-5.9.tar.gz
  $ tar xf PyQt5_gpl-5.9.tar.gz
  $ cd PyQt5_gpl-5.9
  $ python3 configure.py --sysroot=$HOME/local --qmake $HOME/local/Qt5.9.1/5.9.1/gcc_64/bin/qmake --sip $HOME/local/bin/sip --sip-incdir $HOME/local/include --sipdir $HOME/local/share/sip/PyQt5 --bindir $HOME/local/bin --destdir $HOME/local/lib/python3/dist-packages --stubsdir $HOME/local/lib/python3/dist-packages/PyQt5 --configuration myconfig.txt
  $ make -j
  $ make install
  ```

- and the configuration file myconfig.txt has following contents

  ```python
  # The target Python installation.
  py_platform = linux
  py_inc_dir = /usr/include/python%(py_major).%(py_minor)
  qt_shared = True

  [Qt 5.9.1]
  ```

- PyQt5 modules have been installed in *$HOME/local/lib/python3/dist-packages*
