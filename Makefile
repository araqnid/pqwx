-include local_build.mk

all: pqwx test_catalogue

vcs_version.mk pqwx_version.h: FORCE
	@./update_vcs_version vcs_version.mk pqwx_version.h
-include vcs_version.mk

PG_CONFIG := pg_config
WX_CONFIG := wx-config

ifeq (pg_config,$(PG_CONFIG))
debversion := $(shell cat /etc/debian_version 2>/dev/null)
ifneq (,$(debversion))
LOCAL_CXXFLAGS += -DUSE_DEBIAN_PGCLUSTER
endif
else
LOCAL_LIBS += -Wl,-rpath,$(shell $(PG_CONFIG) --libdir)
endif

ifdef RELEASE
WX_CONFIG_FLAGS =
VARIANT_CXXFLAGS = -g -O
else
WX_CONFIG_FLAGS = --debug=yes
VARIANT_CXXFLAGS = -DPQWX_DEBUG -ggdb
endif

WX_MODULES := base core xrc adv html

CXXFLAGS := $(LOCAL_CXXFLAGS) $(VARIANT_CXXFLAGS) -I$(shell $(PG_CONFIG) --includedir) $(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) --cxxflags $(WX_MODULES))
LDFLAGS := $(LOCAL_LIBS) -L$(shell $(PG_CONFIG) --libdir) -lpq $(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) --libs $(WX_MODULES))
OBJS := pqwx.o pqwx_frame.o object_browser.o database_connection.o resources.o connect_dialogue.o catalogue_index.o object_finder.o
XRC := rc/connect.xrc rc/main.xrc rc/object_finder.xrc rc/object_browser.xrc
SOURCES = $(OBJS:.o=.cpp) catalogue_index.h connect_dialogue.h database_connection.h database_work.h object_browser_database_work.h object_browser.h object_browser_model.h object_finder.h pqwx_frame.h pqwx.h server_connection.h sql_logger.h versioned_sql.h

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

rc/%.c: rc/%.xrc
	wxrc -o $@ -g $^

pqwx.pot: $(SOURCES) $(patsubst rc/%.xrc,rc/%.c,$(XRC))
	xgettext --from-code=UTF-8 -k_ -o $@ $^

pot: pqwx.pot

clean:
	rm -f *.o *.d pqwx test_catalogue vcs_version.mk pqwx_version.h resources.cpp object_browser_sql.h resources.h rc/*.c

.PHONY: FORCE
