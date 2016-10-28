#!/usr/bin/env python

import numpy as np
import sys
import pydagmodel as dm

def main():
    for arg in sys.argv[1:]:
        print "To read DAG file " + arg
        print "Result: "
        print dm.get_dag_stat_from_dag_file(arg)

if __name__ == "__main__":
    main()
