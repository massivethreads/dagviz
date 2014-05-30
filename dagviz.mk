platform?=g
prefix?=$(realpath ../../..)/inst/$(platform)

#username?=huynh
username?=zanton

all: compile
download :
compile : compile_done
install : install_done

exe:=dagviz

compile_done : $(exe)
	touch $@

$(exe) : dagviz.c
	gcc -Wall -g -I/home/$(username)/parallel2/sys/inst/g/include -L/home/$(username)/parallel2/sys/inst/g/lib dagviz.c read.c layout.c draw.c utils.c print.c -Wl,-R/home/$(username)/parallel2/sys/inst/g/lib `pkg-config --cflags --libs gtk+-3.0` -o $(exe) -ldr -lm

install_done: compile_done
	mkdir -p $(prefix)/bin
	ln -sf $(realpath .)/$(exe) $(prefix)/bin/
	touch $@

distclean:
	rm -rf *_done

