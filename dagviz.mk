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
	gcc -Wall -g dagviz.c `pkg-config --cflags --libs gtk+-3.0` -o $(exe)

install_done: compile_done
	mkdir -p $(prefix)/bin
	ln -sf $(realpath .)/$(exe) $(prefix)/bin/
	touch $@

distclean:
	rm -rf *_done

