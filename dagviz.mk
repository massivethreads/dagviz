platform ?= g
parallel2_dir ?= ${HOME}/parallel2
prefix ?= $(parallel2_dir)/sys/inst/$(platform)

CC ?= gcc
PKGCONFIG = $(shell which pkg-config)
GTK_CFLAGS = $(shell $(PKGCONFIG) --cflags gtk+-3.0)
GTK_LDFLAGS = $(shell $(PKGCONFIG) --libs gtk+-3.0)
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)

HDR = dagviz.h callback.h 
AUX_SRC = dagviz.gresource.xml
SRC = read.c layout.c draw.c utils.c print.c view_dag.c view_dag_box.c view_timeline.c view_timeline_ver.c view_paraprof.c control.c graphs.c
BUILT_SRC = resources.c
#EXE_SRC = dagviz_main.c dagprof.c

OBJS = $(BUILT_SRC:.c=.o) $(SRC:.c=.o)


CFLAGS += $(cflags)
CFLAGS += -Wall -Wextra -g -O3
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

exes := dagviz dagprof

all: $(exes)

dagprof: dagprof.o $(OBJS)
	$(CC) -o $@ dagprof.o $(OBJS) $(GTK_LDFLAGS) $(LDFLAGS)

dagviz: dagviz_main.o $(HDR) $(AUX_SRC) $(OBJS)
	$(CC) -o $@ dagviz_main.o $(OBJS) $(GTK_LDFLAGS) $(LDFLAGS)

dagprof.o: dagprof.c
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<

dagviz_main.o: dagviz_main.c $(HDR)
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<

%.o: %.c dagviz.h
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<

resources.c: dagviz.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies dagviz.gresource.xml)
	$(GLIB_COMPILE_RESOURCES) dagviz.gresource.xml --target=$@ --sourcedir=. --generate-source


install:
	cp gui/128x128/dagviz_icon.png /usr/share/icons/hicolor/128x128/apps/dagviz.png
	cp gui/dag_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-dag.svg
	cp gui/viewport_division_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-viewport-division.svg
	cp gui/dag_boxes_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-dag-boxes.svg
	cp gui/paraprof_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-paraprof.svg
	gtk-update-icon-cache-3.0 -f -t /usr/share/icons/hicolor/
	cp dagviz /usr/bin/dagviz
	cp gui/dagviz.desktop /usr/share/applications/
#	cp gui/128x128/dag_icon.png /usr/share/icons/hicolor/128x128/apps/dagviz-dag.png

uninstall:
	rm -f /usr/share/icons/hicolor/128x128/apps/dagviz.png
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-dag.svg
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-viewport-division.svg
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-dag-boxes.svg
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-paraprof.svg
	gtk-update-icon-cache-3.0 -f -t /usr/share/icons/hicolor/
	rm -f /usr/bin/dagviz
	rm -f /usr/share/applications/dagviz.desktop
#	rm -f /usr/share/icons/hicolor/128x128/apps/dagviz-dag.png

clean:
	rm -f $(BUILT_SRC)
	rm -f $(OBJS)
	rm -f $(exes)

