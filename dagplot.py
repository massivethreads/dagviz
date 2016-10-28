#!/usr/bin/env python

import sys
import pydagmodel as dm
import numpy as np
import matplotlib
matplotlib.use('WXAgg')
#matplotlib.use('GTK3Cairo')
import matplotlib.pyplot as plt

def main():
    D = []
    name = []
    work = []
    delay = []
    nowork = []
    for arg in sys.argv[1:]:
        d = dm.get_dag_stat_from_dag_file(arg)
        D.append(d)
        name.append(d["name"])
        work.append(d["work"])
        delay.append(d["delay"])
        nowork.append(d["nowork"])
    workdelay = []
    for i in range(len(work)):
        workdelay.append(work[i] + delay[i])

    fig, axes = plt.subplots(nrows=2, ncols=3)
    index = np.arange(len(D))
    bar_width = 0.4
    plt.setp(axes, xticks=index+bar_width/2., xticklabels=range(len(D)))
    ax00, ax01, ax02, ax10, ax11, ax12 = axes.flat
    
    ax00.bar(index, work, bar_width, alpha=0.8, label='work', color='r')
    ax00.bar(index, delay, bar_width, alpha=0.8, label='delay', color='g', bottom=work)
    ax00.bar(index, nowork, bar_width, alpha=0.8, label='no-work', color='b', bottom=workdelay)
    ax00.set_xticks(index)
    ax00.set_xticklabels(name)
    plt.setp(ax00.get_xticklabels(), rotation=-35, horizontalalignment='left')
    ax00.set_ylabel('cumul. clocks')
    #ax00.legend()
    ax00.set_title('work - delay - no-work')

    counter_0 = []
    counter_1 = []
    counter_2 = []
    counter_3 = []
    counter_ipc = []
    for i in range(len(D)):
        counter_0.append(D[i]["counters"][0])
        counter_1.append(D[i]["counters"][1])
        counter_2.append(D[i]["counters"][2])
        counter_3.append(D[i]["counters"][3])
        counter_ipc.append(float(D[i]["counters"][0]) / float(D[i]["counters"][1]))
        
    ax01.bar(index, counter_0, bar_width, label='counter 0', color='r')
    ax01.set_ylabel('counts')
    #ax01.legend()
    ax01.set_title('counter 0')

    ax02.bar(index, counter_1, bar_width, label='counter 1', color='r')
    ax02.set_ylabel('counts')
    #ax02.legend()
    ax02.set_title('counter 1')

    ax10.bar(index, counter_2, bar_width, label='counter 2', color='r')
    ax10.set_ylabel('counts')
    #ax10.legend()
    ax10.set_title('counter 2')

    ax11.bar(index, counter_3, bar_width, label='counter 3', color='r')
    ax11.set_ylabel('counts')
    #ax11.legend()
    ax11.set_title('counter 3')

    ax12.bar(index, counter_ipc, bar_width, label='ipc', color='r')
    ax12.set_ylabel('counts')
    #ax12.legend()
    ax12.set_title('ipc')

    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    main()
