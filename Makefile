#username?=huynh
username?=zanton

all:
	gcc -g -o dagviz dagviz.c read.c layout.c draw.c utils.c print.c -I/home/$(username)/parallel2/sys/inst/g/include -L/home/$(username)/parallel2/sys/inst/g/lib -Wl,-R/home/$(username)/parallel2/sys/inst/g/lib `pkg-config --cflags --libs gtk+-3.0` -ldr -lm
