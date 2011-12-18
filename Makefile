-include local_build.mk

all: pqwx test_catalogue

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

WX_MODULES := base core xrc adv
CXXFLAGS := $(LOCAL_CXXFLAGS) -I$(shell pg_config --includedir) $(shell wx-config $(WX_CONFIG_FLAGS) --cxxflags $(WX_MODULES))
LDFLAGS := -L$(shell pg_config --libdir) -lpq $(shell wx-config $(WX_CONFIG_FLAGS) --libs $(WX_MODULES))
OBJS := pqwx.o pqwx_frame.o object_browser.o database_connection.o resources.o connect_dialogue.o catalogue_index.o
XRC := rc/connect.xrc rc/main.xrc

pqwx: $(OBJS)
	g++ $(LDFLAGS) -o $@ $^

test_catalogue: catalogue_index.o test_catalogue.o
	g++ $(LDFLAGS) -o $@ $^

-include $(OBJS:.o=.d) test_catalogue.d

%.o: %.cpp
	g++ $(CXXFLAGS) -c -o $@ $*.cpp
	@g++ $(CXXFLAGS) -MM -o $*.d $*.cpp

resources.cpp: $(XRC)
	wxrc -c -o $*.cpp $(XRC)

resources.h: $(XRC)
	wxrc -c -e -o $*.cpp $(XRC)

object_browser_sql.h: object_browser.sql format_sql_header
	./format_sql_header object_browser.sql $@

object_browser.o: object_browser_sql.h

clean:
	rm -f *.o *.d pqwx test_catalogue vcs_version.mk pqwx_version.h resources.cpp object_browser_sql.h

.PHONY: FORCE
