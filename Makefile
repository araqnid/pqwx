-include local_build.mk

host_system := $(shell uname -s)
ifneq (,$(findstring MINGW,$(host_system)))
dotEXE = .exe
else
dotEXE =
endif

all: pqwx$(dotEXE) test_catalogue$(dotEXE) dump_catalogue$(dotEXE)

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
endif

ifdef RELEASE
VARIANT_WXCONFIG_FLAGS = --debug=no
VARIANT_CXXFLAGS = -O
else
VARIANT_WXCONFIG_FLAGS = --debug=yes
VARIANT_CXXFLAGS = -ggdb
endif

WX_MODULES := stc xrc adv html xml core base
WXRC := $(shell $(WX_CONFIG) --utility=wxrc)

CXXFLAGS := $(LOCAL_CXXFLAGS) $(VARIANT_CXXFLAGS) -Wall -I$(shell $(PG_CONFIG) --includedir) $(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) $(VARIANT_WXCONFIG_FLAGS) --cxxflags $(WX_MODULES))
LDFLAGS := $(LOCAL_LDFLAGS)
LIBS := -L$(shell $(PG_CONFIG) --libdir) -lpq $(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) $(VARIANT_WXCONFIG_FLAGS) --libs $(WX_MODULES))
XRC := rc/connect.xrc rc/main.xrc rc/object_finder.xrc rc/object_browser.xrc rc/dependencies_view.xrc
PQWX_SOURCES = pqwx.cpp pqwx_frame.cpp object_browser.cpp database_connection.cpp resources.cpp connect_dialogue.cpp catalogue_index.cpp object_finder.cpp static_resources_yml.cpp dependencies_view.cpp database_work.cpp object_browser_sql.cpp dependencies_view_sql.cpp object_browser_scripts.cpp scripts_notebook.cpp results_notebook.cpp script_editor.cpp execution_lexer.cpp
PQWX_HEADERS = catalogue_index.h connect_dialogue.h database_connection.h database_work.h object_browser_database_work.h object_browser.h object_browser_model.h object_finder.h pqwx_frame.h pqwx.h server_connection.h sql_logger.h versioned_sql.h scripts_notebook.h results_notebook.h script_editor.h script_events.h execution_lexer.h script_query_work.h pg_error.h database_event_type.h
SOURCES = $(PQWX_SOURCES) test_catalogue.cpp dump_catalogue.cpp
PQWX_OBJS = $(PQWX_SOURCES:.cpp=.o)
ifneq (,$(findstring MINGW,$(host_system)))
PQWX_OBJS += pqwx_rc.o
endif

wx_flavour.h build_settings: FORCE
	@settings='$(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) $(VARIANT_WXCONFIG_FLAGS) --selected-config)'; \
	if test x"$$settings" != x"`cat build_settings 2>/dev/null`" ; then \
		echo "WX_FLAVOUR = $$settings"; \
		echo $$settings > build_settings; \
		echo "#define WX_FLAVOUR \"$$settings\"" > wx_flavour.h; \
	fi

pqwx$(dotEXE): $(PQWX_OBJS)
	g++ $(LDFLAGS) -o $@ $^ $(LIBS)

test_catalogue$(dotEXE): catalogue_index.o test_catalogue.o
	g++ $(LDFLAGS) -o $@ $^ $(LIBS)

dump_catalogue$(dotEXE): dump_catalogue.o object_browser_sql.o
	g++ $(LDFLAGS) -o $@ $^ $(LIBS)

-include $(SOURCES:.cpp=.d)

pqwx_rc.o: pqwx.rc pqwx_version.h
	$(shell $(WX_CONFIG) --rescomp) pqwx.rc $@

%.o: %.cpp build_settings
	g++ $(CXXFLAGS) -c -o $@ $*.cpp
	@g++ $(CXXFLAGS) -MM -o $*.d $*.cpp

resources.cpp: $(XRC)
	$(WXRC) -c -o $*.cpp $(XRC)

resources.h: $(XRC)
	$(WXRC) -c -e -o $*.cpp $(XRC)

object_browser_sql.cpp: object_browser.sql format_sql_header
	./format_sql_header -c ObjectBrowserSql -f 'ObjectBrowser::GetSqlDictionary' -h object_browser.h object_browser.sql $@

dependencies_view_sql.cpp: dependencies_view.sql format_sql_header
	./format_sql_header -c DependenciesViewSql  -f 'DependenciesView::GetSqlDictionary' -h dependencies_view.h dependencies_view.sql $@

static_resources_yml.cpp: static_resources.yml
	./format_static_resources -f 'InitStaticResources' -o $@ -d static_resources.d static_resources.yml

postgresql_wordlists_yml.h: postgresql_wordlists.yml
	./format_wordlists $^

script_editor.o: postgresql_wordlists_yml.h

-include static_finder_resources.d

rc/%.c: rc/%.xrc
	wxrc -o $@ -g $^

pqwx.pot: $(PQWX_SOURCES) $(patsubst rc/%.xrc,rc/%.c,$(XRC))
	xgettext --from-code=UTF-8 -k_ -o $@ $^

pot: pqwx.pot

pqwx-appicon-%-8.png: pqwx-appicon-%.png
	convert -depth 8 $^ $@

pqwx-appicon.ico: pqwx-appicon-16-8.png pqwx-appicon-32-8.png pqwx-appicon-64-8.png pqwx-appicon-16.png pqwx-appicon-32.png pqwx-appicon-64.png
	icotool -c -o $@ $^

clean:
	rm -f *.o *.d pqwx test_catalogue vcs_version.mk pqwx_version.h resources.cpp resources.h rc/*.c build_settings wx_flavour.h object_browser_sql.cpp dependencies_view_sql.cpp postgresql_wordlists_yml.h

.PHONY: FORCE
