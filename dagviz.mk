platform ?= g
parallel2_dir ?= ${HOME}/parallel2
prefix ?= $(parallel2_dir)/sys/inst/$(platform)

CC ?= gcc
PKGCONFIG = $(shell which pkg-config)
CFLAGS = $(shell $(PKGCONFIG) --cflags gtk+-3.0)
LDFLAGS = $(shell $(PKGCONFIG) --libs gtk+-3.0)
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)

HDR = dagviz.h
AUX_SRC = control.c dagviz.gresource.xml
SRC = dagviz.c read.c layout.c draw.c utils.c print.c view_dag.c view_dag_box.c view_timeline.c view_timeline_ver.c view_paraprof.c process.c 
BUILT_SRC = resources.c

OBJS = $(BUILT_SRC:.c=.o) $(SRC:.c=.o)


CFLAGS += $(cflags)
CFLAGS += -Wall -Wextra -g
#CFLAGS += -fno-stack-protector
#CFLAGS += -DDV_ENABLE_LIBUNWIND
#CFLAGS += -DDV_ENABLE_BFD
CFLAGS += -I$(prefix)/include
CFLAGS += -DDAG_RECORDER=2 -DDAG_RECORDER_INLINE_INSTRUMENTATION

LDFLAGS += $(ldflags)
#LDFLAGS += -lunwind
#LDFLAGS += -lbfd
LDFLAGS += -Wl,--export-dynamic
LDFLAGS += -L$(prefix)/lib -Wl,-R$(prefix)/lib
LDFLAGS += -ldr -lm -lpthread

exe := dagviz


all: $(exe)

$(exe): $(HDR) $(AUX_SRC) $(OBJS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $<

resources.c: dagviz.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies dagviz.gresource.xml)
	$(GLIB_COMPILE_RESOURCES) dagviz.gresource.xml --target=$@ --sourcedir=. --generate-source

clean:
	rm -f $(BUILT_SRC)
	rm -f $(OBJS)
	rm -f $(exe)

