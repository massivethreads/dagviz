parallel2_dir ?= $(HOME)/parallel2

cflags_w ?= -g
cflags_uw ?= -DENABLE_LIBUNWIND
ldflags_uw ?= -lunwind -lbfd

include dagviz.mk
