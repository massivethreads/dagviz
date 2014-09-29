platform?=g
parallel2_dir?=$(realpath ../../../..)
prefix?=$(parallel2_dir)/sys/inst/$(platform)

CC:=gcc

cflags_w?=-g #-Wall

CFLAGS+=$(cflags_w)
CFLAGS+=-I$(prefix)/include
CFLAGS+=-DDAG_RECORDER=2 -DDAG_RECORDER_INLINE_INSTRUMENTATION

ldflags_uw?= #-lunwind
LDFLAGS+=-L$(prefix)/lib -Wl,-R$(prefix)/lib
LDFLAGS+=`pkg-config --cflags --libs gtk+-3.0`
LDFLAGS+=-ldr -lm $(ldflags_uw)

all: compile
download :
compile : compile_done
install : install_done

exe:=dagviz

compile_done : $(exe)
	touch $@

$(exe) : dagviz.c
	$(CC) $(CFLAGS) dagviz.c read.c layout.c draw.c utils.c print.c view_glike.c view_bbox.c view_timeline.c $(LDFLAGS) -o $@

install_done: compile_done
	mkdir -p $(prefix)/bin
	ln -sf $(realpath .)/$(exe) $(prefix)/bin/
	touch $@

distclean:
	rm -rf *_done

