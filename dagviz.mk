platform ?= g
parallel2_dir ?= $(realpath ../../../..)
prefix ?= $(parallel2_dir)/sys/inst/$(platform)

CC := gcc

cflags_1 ?= -Wall -Wextra -g #-fno-stack-protector
cflags_2 ?= #-DDV_ENABLE_LIBUNWIND
cflags_3 ?= #-DDV_ENABLE_BFD

CFLAGS += $(cflags_1)
CFLAGS += -I$(prefix)/include
CFLAGS += -DDAG_RECORDER=2 -DDAG_RECORDER_INLINE_INSTRUMENTATION
CFLAGS += $(cflags_2)
CFLAGS += $(cflags_3)

ldflags_1 ?=
ldflags_2 ?= #-lunwind
ldflags_3 ?= #-lbfd

LDFLAGS += -L$(prefix)/lib -Wl,-R$(prefix)/lib
LDFLAGS += `pkg-config --cflags --libs gtk+-3.0`
LDFLAGS += -ldr -lm $(ldflags_uw)
LDFLAGS += $(ldflags_1)
LDFLAGS += $(ldflags_2)
LDFLAGS += $(ldflags_3)

all: compile
download :
compile : compile_done
install : install_done

exe := dagviz

compile_done : $(exe)
	touch $@

$(exe) : dagviz.h dagviz.c read.c layout.c draw.c utils.c print.c view_dag.c view_dag_box.c view_timeline.c view_timeline_ver.c view_paraprof.c process.c control.c
	$(CC) $(CFLAGS) dagviz.c read.c layout.c draw.c utils.c print.c view_dag.c view_dag_box.c view_timeline.c view_timeline_ver.c view_paraprof.c $(LDFLAGS) -o $@

install_done: compile_done
	mkdir -p $(prefix)/bin
	ln -sf $(realpath .)/$(exe) $(prefix)/bin/
	touch $@

distclean:
	rm -rf *_done

