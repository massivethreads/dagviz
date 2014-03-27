platform?=g
prefix?=$(realpath ../../..)/inst/$(platform)

all: compile
download :
compile : compile_done
install : install_done

exe:=dagviz

compile_done : $(exe)
	touch $@

$(exe) : dagviz.c
	gcc -Wall -g -I/home/zanton/parallel2/sys/inst/g/include -L/home/zanton/parallel2/sys/inst/g/lib dagviz.c print.c layout.c draw.c utils.c -Wl,-R/home/zanton/parallel2/sys/inst/g/lib `pkg-config --cflags --libs gtk+-3.0` -o $(exe) -ldr

install_done: compile_done
	mkdir -p $(prefix)/bin
	ln -sf $(realpath .)/$(exe) $(prefix)/bin/
	touch $@

distclean:
	rm -rf *_done

