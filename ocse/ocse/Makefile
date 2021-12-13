srcdir = $(PWD)
COMMON_DIR=../common
include Makefile.vars
include Makefile.rules

SRCS = $(wildcard *.c)
OBJS = $(subst .c,.o,$(SRCS)) debug.o tlx_interface.o utils.o

all: ocse

ocse: $(OBJS)
	$(call Q,CC, $(CC) $(CFLAGS) -o $@ $^ -lpthread -lm, $@)

clean:
	rm -rf *.[od] *.d-e gmon.out ocse

.PHONY: clean all
