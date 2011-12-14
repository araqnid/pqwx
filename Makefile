-include local_build.mk

ifdef DEBUG
WX_CONFIG_FLAGS := --debug=yes
LOCAL_CXXFLAGS := -DPQWX_DEBUG -ggdb
else
WX_CONFIG_FLAGS :=
LOCAL_CXXFLAGS := -g -O
endif

CXXFLAGS := $(LOCAL_CXXFLAGS) -I$(shell pg_config --includedir) $(shell wx-config $(WX_CONFIG_FLAGS) --cxxflags)
LDFLAGS := -L$(shell pg_config --libdir) -lpq $(shell wx-config $(WX_CONFIG_FLAGS) --libs)
OBJS := server_connection.o pqwx.o pqwx_frame.o object_browser.o database_connection.o

pqwx: $(OBJS)
	g++ $(LDFLAGS) -o $@ $^

-include $(OBJS:.o=.d)

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $*.cpp
	@g++ $(CXXFLAGS) -MM -o $*.d $*.cpp

clean:
	rm -f *.o *.d pqwx
