srcdir = $(PWD)
COMMON_DIR=../../common
LIBOCXL_DIR=../../libocxl
include Makefile.vars
include Makefile.rules

SRCS=$(wildcard *.c)
OBJS=$(subst .c,.o,$(SRCS)) TestAFU_config.o
TESTS=$(subst .c,,$(SRCS))
DEPS=TestAFU_config.o $(LIBOCXL_DIR)/libocxl.a $(LIBOCXL_DIR)/libocxl_lpc.a
all: misc/ocxl.h $(TESTS)

CHECK_HEADER = $(shell echo \\\#include\ $(1) | $(CC) $(CFLAGS) -E - > /dev/null 2>&1 && echo y || echo n)

misc/ocxl.h:
ifeq ($(call CHECK_HEADER,"<misc/ocxl.h>"),n)
	$(call Q,CURL $(COMMON_DIR)/misc/ocxl.h, mkdir $(COMMON_DIR)/misc 2>/dev/null; curl -o $(COMMON_DIR)/misc/ocxl.h -s http://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/plain/include/uapi/misc/ocxl.h)
endif

.SECONDEXPANSION:

$(TESTS) : $(addsuffix .o,$$@) $(DEPS)
	$(call Q,CC, $(CC) $^ -L$(LIBOCXL_DIR) -I$(COMMON_DIR) -I$(LIBOCXL_DIR) -o $@ -lpthread, $@)

TestAFU_config.o :

$(LIBOCXL_DIR)/libocxl.a:
	@$(MAKE) -C $(LIBOCXL_DIR)

clean:
	@$(MAKE) -C $(LIBOCXL_DIR) clean
	rm -f *.o *.d gmon.out $(TESTS)
