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
OBJS := pqwx.o pqwx_frame.o object_browser.o database_connection.o resources.o connect_dialogue.o catalogue_index.o object_finder.o object_finder_resources_yml.o dependencies_view.o database_work.o
XRC := rc/connect.xrc rc/main.xrc rc/object_finder.xrc rc/object_browser.xrc rc/dependencies_view.xrc
SOURCES = $(OBJS:.o=.cpp) catalogue_index.h connect_dialogue.h database_connection.h database_work.h object_browser_database_work.h object_browser.h object_browser_model.h object_finder.h pqwx_frame.h pqwx.h server_connection.h sql_logger.h versioned_sql.h

wx_flavour.h build_settings: FORCE
	@settings='$(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) --selected-config)'; \
	if test x"$$settings" != x"`cat build_settings 2>/dev/null`" ; then \
		echo "WX_FLAVOUR = $$settings"; \
		echo $$settings > build_settings; \
		echo "#define WX_FLAVOUR \"$$settings\"" > wx_flavour.h; \
	fi

pqwx: $(OBJS)
	g++ $(LDFLAGS) -o $@ $^

test_catalogue: catalogue_index.o test_catalogue.o
	g++ $(LDFLAGS) -o $@ $^

-include $(OBJS:.o=.d) test_catalogue.d

%.o: %.cpp build_settings
	g++ $(CXXFLAGS) -c -o $@ $*.cpp
	@g++ $(CXXFLAGS) -MM -o $*.d $*.cpp

resources.cpp: $(XRC)
	wxrc -c -o $*.cpp $(XRC)

resources.h: $(XRC)
	wxrc -c -e -o $*.cpp $(XRC)

object_browser_sql.h: object_browser.sql format_sql_header
	./format_sql_header -c ObjectBrowserSql object_browser.sql $@

dependencies_view_sql.h: dependencies_view.sql format_sql_header
	./format_sql_header -c DependenciesViewSql dependencies_view.sql $@

object_browser.o: object_browser_sql.h

dependencies_view.o: dependencies_view_sql.h

object_finder_resources_yml.cpp: object_finder_resources.yml
	./format_static_resources -f 'InitObjectFinderResources' -o $@ -d object_finder_resources.d object_finder_resources.yml

-include object_finder_resources.d

rc/%.c: rc/%.xrc
	wxrc -o $@ -g $^

pqwx.pot: $(SOURCES) $(patsubst rc/%.xrc,rc/%.c,$(XRC))
	xgettext --from-code=UTF-8 -k_ -o $@ $^

pot: pqwx.pot

clean:
	rm -f *.o *.d pqwx test_catalogue vcs_version.mk pqwx_version.h resources.cpp object_browser_sql.h resources.h rc/*.c build_settings wx_flavour.h

.PHONY: FORCE
