all:
	gcc -g -o dagviz dagviz.c read.c layout.c draw.c utils.c print.c -I/home/zanton/parallel2/sys/inst/g/include -L/home/zanton/parallel2/sys/inst/g/lib -Wl,-R/home/zanton/parallel2/sys/inst/g/lib `pkg-config --cflags --libs gtk+-3.0` -ldr -lm
