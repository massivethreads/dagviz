platform ?= g
parallel2_dir ?= ${HOME}/parallel2
prefix ?= $(parallel2_dir)/sys/inst/$(platform)

CC ?= gcc
PKGCONFIG = $(shell which pkg-config)
GTK_CFLAGS = $(shell $(PKGCONFIG) --cflags gtk+-3.0)
GTK_LDFLAGS = $(shell $(PKGCONFIG) --libs gtk+-3.0)
GLIB_COMPILE_RESOURCES = $(shell $(PKGCONFIG) --variable=glib_compile_resources gio-2.0)

DM_HDR = dagmodel.h
DM_SRC = dagmodel.c
DM_OBJS = $(DM_SRC:.c=.o)

DV_HDR = dagviz.h callback.h 
DV_AUX_SRC = dagviz.gresource.xml interface.c
DV_SRC = layout.c draw.c view_dag.c view_dag_box.c view_timeline.c view_timeline_ver.c view_paraprof.c control.c graphs.c
BUILT_SRC = resources.c
DV_OBJS = $(BUILT_SRC:.c=.o) $(DV_SRC:.c=.o)

DSv1_HDR = dagstat_v1.h
DS_HDR = dagstat.h
DS_SRC = 
DS_OBJS = $(DS_SRC:.c=.o)

EXE_SRC = dagviz.c dagprof.c dagstat_v1.c dagstat.c
OBJS = $(DM_OBJS) $(DV_OBJS) $(DS_OBJS) $(EXE_SRC:.c=.o)


CFLAGS += $(cflags)
CFLAGS += $(cflags_2) $(cflags_3)
CFLAGS += -Wall -Wextra -g -O3
#CFLAGS += -fno-stack-protector
#CFLAGS += -DDV_ENABLE_LIBUNWIND
#CFLAGS += -DDV_ENABLE_BFD
CFLAGS += -I$(prefix)/include
CFLAGS += -DDAG_RECORDER=2 -DDAG_RECORDER_INLINE_INSTRUMENTATION

LDFLAGS += $(ldflags)
LDFLAGS += $(ldflags_2) $(ldflags_3)
#LDFLAGS += -lunwind
#LDFLAGS += -lbfd
LDFLAGS += -Wl,--export-dynamic
LDFLAGS += -L$(prefix)/lib -Wl,-R$(prefix)/lib
LDFLAGS += -ldr -lm -lpthread

exes := $(EXE_SRC:.c=)

all: $(exes)


$(DM_OBJS): %.o: %.c $(DM_HDR)
	$(CC) -c -o $@ $(CFLAGS) $<


dagstat_v1.o: dagstat_v1.c $(DSv1_HDR)
	$(CC) -c -o $@ $(CFLAGS) $<

dagstat.o: dagstat.c $(DS_HDR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(DS_OBJS): %.o: %.c $(DS_HDR)
	$(CC) -c -o $@ $(CFLAGS) $<


dagviz: dagviz.o $(DV_OBJS) $(DM_OBJS)
	$(CC) -o $@ dagviz.o $(DV_OBJS) $(DM_OBJS) $(GTK_LDFLAGS) $(LDFLAGS)

dagprof: dagprof.o $(DV_OBJS) $(DM_OBJS)
	$(CC) -o $@ dagprof.o $(DV_OBJS) $(DM_OBJS) $(GTK_LDFLAGS) $(LDFLAGS)

dagstat_v1: dagstat_v1.o $(DS_OBJS) $(DM_OBJS)
	$(CC) -o $@ dagstat_v1.o $(DS_OBJS) $(DM_OBJS) $(LDFLAGS)

dagstat: dagstat.o $(DS_OBJS) $(DM_OBJS)
	$(CC) -o $@ dagstat.o $(DS_OBJS) $(DM_OBJS) $(LDFLAGS)


dagviz.o: dagviz.c $(DV_HDR) $(DV_AUX_SRC) 
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<

dagprof.o: dagprof.c $(DV_HDR) 
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<

$(DV_OBJS): %.o: %.c $(DV_HDR)
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<


resources.c: dagviz.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies dagviz.gresource.xml)
	$(GLIB_COMPILE_RESOURCES) dagviz.gresource.xml --target=$@ --sourcedir=. --generate-source


install:
	cp gui/128x128/dagviz_icon.png /usr/share/icons/hicolor/128x128/apps/dagviz.png
	cp gui/dag_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-dag.svg
	cp gui/viewport_division_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-viewport-division.svg
	cp gui/dag_boxes_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-dag-boxes.svg
	cp gui/paraprof_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-paraprof.svg
	cp gui/critical_path_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-critical-path.svg
	cp gui/compute_critical_path_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-critical-path-compute.svg
	cp gui/view_settings_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-view-settings.svg
	cp gui/plus_button_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-plus-button.svg
	cp gui/minus_button_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-minus-button.svg
	cp gui/fit_button_icon.svg /usr/share/icons/hicolor/scalable/apps/dagviz-fit-button.svg
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
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-critical-path.svg
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-critical-path-compute.svg
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-view-settings.svg
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-plus-button.svg
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-minus-button.svg
	rm -f /usr/share/icons/hicolor/scalable/apps/dagviz-fit-button.svg
	gtk-update-icon-cache-3.0 -f -t /usr/share/icons/hicolor/
	rm -f /usr/bin/dagviz
	rm -f /usr/share/applications/dagviz.desktop
#	rm -f /usr/share/icons/hicolor/128x128/apps/dagviz-dag.png

clean:
	rm -f $(BUILT_SRC)
	rm -f $(OBJS)

cleanall:
	rm -f $(BUILT_SRC)
	rm -f $(OBJS)
	rm -f $(exes)

