all:
	gcc -Wall -g -o dagviz dagviz.c check.c print.c layout.c draw.c -I/home/zanton/parallel2/sys/inst/g/include -L/home/zanton/parallel2/sys/inst/g/lib -Wl,-R/home/zanton/parallel2/sys/inst/g/lib `pkg-config --cflags --libs gtk+-3.0` -ldr
