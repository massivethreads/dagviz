#!/bin/bash

rm -r build
CFLAGS="-Wno-strict-prototypes" python setup.py build
sudo python setup.py install
