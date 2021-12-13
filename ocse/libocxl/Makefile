srcdir = $(PWD)
COMMON_DIR=../common
include Makefile.vars
include Makefile.rules

OBJS = debug.o utils.o

all: libocxl.so libocxl.a libocxl_lpc.so libocxl_lpc.a

CHECK_HEADER := $(shell echo \\\#include\ $(1) | $(CC) $(CFLAGS) -E - > /dev/null 2>&1 && echo y || echo n)

$(COMMON_DIR)/misc/ocxl.h:
	$(call Q,CURL $(COMMON_DIR)/misc/ocxl.h, mkdir $(COMMON_DIR)/misc 2>/dev/null; curl -o $(COMMON_DIR)/misc/ocxl.h -s https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/include/uapi/misc/ocxl.h)
# $(COMMON_DIR)/misc/ocxl.h:
# ifeq ($(call CHECK_HEADER,"<misc/cxl.h>"),n)
# 	$(info CALLING CURL)
# 	$(call Q,CURL $(COMMON_DIR)/misc/ocxl.h, mkdir $(COMMON_DIR)/misc 2>/dev/null; curl -o $(COMMON_DIR)/misc/ocxl.h -s https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/include/uapi/misc/ocxl.h)
# endif

libocxl.o: $(COMMON_DIR)/misc/ocxl.h

libocxl.so: libocxl.o $(OBJS)
	$(call Q,CC, $(CC) $(CFLAGS) -shared $^ -o $@, $@) -Wl,--version-script symver.map

libocxl.a: libocxl.o $(OBJS)
	$(call Q,AR, $(AR) rcs $@ $^, $@)

libocxl_lpc.o: $(COMMON_DIR)/misc/ocxl.h

libocxl_lpc.so: libocxl_lpc.o $(OBJS)
	$(call Q,CC, $(CC) $(CFLAGS) -shared $^ -o $@, $@) -Wl,--version-script symver_lpc.map

libocxl_lpc.a: libocxl_lpc.o $(OBJS)
	$(call Q,AR, $(AR) rcs $@ $^, $@)

clean:
	rm -f *.[od] $(COMMON_DIR)/*.[od] gmon.out libocxl.so libocxl.a libocxl_lpc.so libocx_lpcl.a $(COMMON_DIR)/misc/ocxl.h

.PHONY: clean all
