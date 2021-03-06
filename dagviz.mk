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
DM_SHARED_OBJS = $(DM_SRC:.c=.so)

DREN_CPP_HDR = qt/dagrenderer.h qt/dagrenderer.pro 
DREN_CPP_SRC = qt/dagrenderer.cpp
DREN_CPP_OBJS = $(patsubst qt/%.cpp,qt/lib%.so,$(DREN_CPP_SRC))
DREN_SIP_HDR = sip/configure.py
DREN_SIP_SRC = sip/dagrenderer.sip
DREN_SIP_OBJS = $(patsubst sip/%.sip,sip/%.so,$(DREN_SIP_SRC))

DV_HDR = dagviz-gtk.h callback.h 
DV_AUX_SRC = dagviz-gtk.gresource.xml interface.c
DV_SRC = layout.c draw.c view_dag.c view_dag_box.c view_timeline.c view_timeline_ver.c view_paraprof.c control.c graphs.c
BUILT_SRC = resources.c
DV_OBJS = $(BUILT_SRC:.c=.o) $(DV_SRC:.c=.o)

DSv1_HDR = dagstat_v1.h
DS_HDR = dagstat.h
DS_SRC = 
DS_OBJS = $(DS_SRC:.c=.o)

EXE_SRC = dagviz-gtk.c dagprof.c dagstat_v1.c dagstat.c
OBJS = $(DM_OBJS) $(DV_OBJS) $(DS_OBJS) $(EXE_SRC:.c=.o) $(DM_SHARED_OBJS) $(DREN_CPP_OBJS).* $(DREN_SIP_OBJS)


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

exes := $(EXE_SRC:.c=) dagviz

all: $(exes) $(DREN_CPP_OBJS) $(DREN_SIP_OBJS)

gtk: $(DM_OBJS) $(DV_OBJS) dagviz-gtk
	echo "dagviz-gtk has been compiled"
qt: $(DM_SHARED_OBJS) $(DREN_CPP_OBJS) $(DREN_SIP_OBJS)	
	echo "dagviz-pyqt's requirements $(DM_SHARED_OBJS) $(DREN_CPP_OBJS) $(DREN_SIP_OBJS) have been compiled"


$(DM_OBJS): %.o: %.c $(DM_HDR) $(DM_SHARED_OBJS)
	$(CC) -c -o $@ $(CFLAGS) $<

$(DM_SHARED_OBJS): %.so: %.c $(DM_HDR)
	$(CC) -shared -export-dynamic -fPIC -o $@ $(CFLAGS) $<


dagstat_v1.o: dagstat_v1.c $(DSv1_HDR)
	$(CC) -c -o $@ $(CFLAGS) $<

dagstat.o: dagstat.c $(DS_HDR)
	$(CC) -c -o $@ $(CFLAGS) $<

$(DS_OBJS): %.o: %.c $(DS_HDR)
	$(CC) -c -o $@ $(CFLAGS) $<


dagviz:
	make qt
	ln -fs dagviz-pyqt dagviz

dagviz-gtk: dagviz-gtk.o $(DV_OBJS) $(DM_OBJS)
	$(CC) -o $@ dagviz-gtk.o $(DV_OBJS) $(DM_OBJS) $(GTK_LDFLAGS) $(LDFLAGS)

dagprof: dagprof.o $(DV_OBJS) $(DM_OBJS)
	$(CC) -o $@ dagprof.o $(DV_OBJS) $(DM_OBJS) $(GTK_LDFLAGS) $(LDFLAGS)

dagstat_v1: dagstat_v1.o $(DS_OBJS) $(DM_OBJS)
	$(CC) -o $@ dagstat_v1.o $(DS_OBJS) $(DM_OBJS) $(LDFLAGS)

dagstat: dagstat.o $(DS_OBJS) $(DM_OBJS)
	$(CC) -o $@ dagstat.o $(DS_OBJS) $(DM_OBJS) $(LDFLAGS)


dagviz-gtk.o: dagviz-gtk.c $(DV_HDR) $(DV_AUX_SRC) 
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<

dagprof.o: dagprof.c $(DV_HDR) 
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<

$(DV_OBJS): %.o: %.c $(DV_HDR)
	$(CC) -c -o $@ $(GTK_CFLAGS) $(CFLAGS) $<


resources.c: dagviz-gtk.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=. --generate-dependencies dagviz-gtk.gresource.xml)
	$(GLIB_COMPILE_RESOURCES) dagviz-gtk.gresource.xml --target=$@ --sourcedir=. --generate-source

$(DREN_CPP_OBJS): $(DREN_CPP_SRC) $(DREN_CPP_HDR)
	cd qt; qmake; make; cd -

$(DREN_SIP_OBJS): $(DREN_SIP_SRC) $(DREN_SIP_HDR)
	cd sip; python3 configure.py; make; cd -


install-obsolete:
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

uninstall-obsolete:
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

