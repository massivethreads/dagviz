platform?=g
prefix?=$(realpath ../../..)/inst/$(platform)

#username?=huynh
#username?=zanton
parallel2_dir:=$(prefix)

all: compile
download :
compile : compile_done
install : install_done

exe:=dagviz

compile_done : $(exe)
	touch $@

# /home/$(username)/parallel2/sys/inst/g
$(exe) : dagviz.c
	gcc -Wall -g -I$(parallel2_dir)/include dagviz.c read.c layout.c draw.c utils.c print.c -L$(parallel2_dir)/lib -Wl,-R$(parallel2_dir)/lib `pkg-config --cflags --libs gtk+-3.0` -o $(exe) -ldr -lm

install_done: compile_done
	mkdir -p $(prefix)/bin
	ln -sf $(realpath .)/$(exe) $(prefix)/bin/
	touch $@

distclean:
	rm -rf *_done

