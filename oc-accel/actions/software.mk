#
# Copyright 2017 International Business Machines
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include $(SNAP_ROOT)/software/config.mk
# Environment variable OCSE_ROOT defined by hardware setup scripts. Use default path if OCSE_ROOT is not defined.
ifndef OCSE_ROOT
OCSE_ROOT=$(abspath ../../ocse)
endif

CFLAGS += -std=c99 -fPIC

LDLIBS += -losnap -locxl -lpthread

LDFLAGS += -Wl,-rpath,$(SNAP_ROOT)/software/lib
LIBS += $(SNAP_ROOT)/software/lib/libosnap.so

ifdef BUILD_SIMCODE
CFLAGS += -D_SIM_
LDFLAGS += -L$(OCSE_ROOT)/libocxl -Wl,-rpath,$(OCSE_ROOT)/libocxl
LIBS += $(OCSE_ROOT)/libocxl/libocxl.so
endif

# This rule should be the 1st one to find (default)
all: all_build

# Include sub-Makefile if there are any
# -include *.mk

# This rule needs to be behind all the definitions above
all_build: $(projs)

$(projs): $(LIBS) $(libs)

$(libs): $(LIBS)

$(SNAP_ROOT)/software/lib/libosnap.so:
	$(MAKE) -C `dirname $@`


# Resolve dependencies to required libraries
#$(projs) $(libs): $(OCSE_ROOT)/libocxl/libocxl.so $(SNAP_ROOT)/software/lib/libosnap.so
#
#$(OCSE_ROOT)/libocxl/libocxl.so $(SNAP_ROOT)/software/lib/libosnap.so:
#	$(MAKE) -C `dirname $@`

### Deactivate existing implicit rule
%: %.c
%: %.sh

### Generic rule to build a tool
%: %.o
	$(CC) $(LDFLAGS) $($(@)_LDFLAGS) $@.o $($(@)_objs) $($(@)_libs) $(LDLIBS) -o $@

%.o: %.c
	$(CC) -c $(CPPFLAGS) $($(@:.o=)_CPPFLAGS) $(CFLAGS) $< -o $@

install: all
	@mkdir -p $(DESTDIR)/bin
	@for f in $(projs); do 					\
		echo "installing $(DESTDIR)/bin/$$f ...";	\
		intall -D -m 755 $$f -T $(DESTDIR)/bin/$$f	\
	done
	@for f in $(libs); do 					\
		echo "installing $(DESTDIR)/lib/$$f ...";	\
		intall -D -m 755 $$f -T $(DESTDIR)/lib/$$f	\
	done

uninstall:
	@for f in $(projs) ; do					\
		echo "removing $(DESTDIR)/bin/$$f ...";		\
		$(RM) $(DESTDIR)/bin/$$f;			\
	done
	@for f in $(libs) ; do					\
		echo "removing $(DESTDIR)/lib/$$f ...";		\
		$(RM) $(DESTDIR)/lib/$$f;			\
	done

clean distclean:
	$(RM) $(projs) $(libs) *.o *.log *.out *~

