-include local_build.mk

all: pqwx

vcs_version.mk pqwx_version.h: FORCE
	@./update_vcs_version vcs_version.mk pqwx_version.h
-include vcs_version.mk

ifdef RELEASE
WX_CONFIG_FLAGS :=
LOCAL_CXXFLAGS := -g -O
else
WX_CONFIG_FLAGS := --debug=yes
LOCAL_CXXFLAGS := -DPQWX_DEBUG -ggdb
endif

CXXFLAGS := $(LOCAL_CXXFLAGS) -I$(shell pg_config --includedir) $(shell wx-config $(WX_CONFIG_FLAGS) --cxxflags)
LDFLAGS := -L$(shell pg_config --libdir) -lpq $(shell wx-config $(WX_CONFIG_FLAGS) --libs)
OBJS := pqwx.o pqwx_frame.o object_browser.o database_connection.o

pqwx: $(OBJS)
	g++ $(LDFLAGS) -o $@ $^

-include $(OBJS:.o=.d)

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $*.cpp
	@g++ $(CXXFLAGS) -MM -o $*.d $*.cpp

clean:
	rm -f *.o *.d pqwx vcs_version.mk pqwx_version.h

.PHONY: FORCE