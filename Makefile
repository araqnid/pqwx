-include local_build.mk

host_system := $(shell uname -s)
ifneq (,$(findstring MINGW,$(host_system)))
dotEXE = .exe
else
dotEXE =
endif

EXECUTABLES = pqwx$(dotEXE) test_catalogue$(dotEXE) dump_catalogue$(dotEXE)

all: $(EXECUTABLES)

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
WX_CONFIG_FLAGS := --version=2.8
WXRC := $(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) --utility=wxrc)

CXXFLAGS := $(LOCAL_CXXFLAGS) $(VARIANT_CXXFLAGS) -Wall -I$(shell $(PG_CONFIG) --includedir) $(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) $(VARIANT_WXCONFIG_FLAGS) --cxxflags $(WX_MODULES))
LDFLAGS := $(LOCAL_LDFLAGS)
LIBS := -L$(shell $(PG_CONFIG) --libdir) -lpq $(shell $(WX_CONFIG) $(WX_CONFIG_FLAGS) $(VARIANT_WXCONFIG_FLAGS) --libs $(WX_MODULES))
XRC := rc/connect.xrc rc/main.xrc rc/object_finder.xrc rc/object_browser.xrc rc/dependencies_view.xrc rc/create_database.xrc rc/preferences.xrc
PQWX_SOURCES = \
	catalogue_index.cpp \
	connect_dialogue.cpp \
	create_database_dialogue.cpp \
	database_connection.cpp \
	database_work.cpp \
	dependencies_view.cpp \
	documents_notebook.cpp \
	execution_lexer.cpp \
	object_browser.cpp \
	object_browser_database_work.cpp \
	object_browser_model.cpp \
	object_browser_scripts.cpp \
	object_browser_scripts_database.cpp \
	object_browser_scripts_function.cpp \
	object_browser_scripts_index.cpp \
	object_browser_scripts_operator.cpp \
	object_browser_scripts_role.cpp \
	object_browser_scripts_schema.cpp \
	object_browser_scripts_sequence.cpp \
	object_browser_scripts_table.cpp \
	object_browser_scripts_tablespace.cpp \
	object_browser_scripts_ts_config.cpp \
	object_browser_scripts_ts_dict.cpp \
	object_browser_scripts_ts_parser.cpp \
	object_browser_scripts_ts_tmpl.cpp \
	object_browser_scripts_type.cpp \
	object_browser_scripts_view.cpp \
	object_finder.cpp \
	pg_tools_registry.cpp \
	pqwx.cpp \
	pqwx_frame.cpp \
	preferences_dialogue.cpp \
	results_notebook.cpp \
	script_editor.cpp \
	script_editor_pane.cpp \
	script_execution.cpp \
	script_query_work.cpp
PQWX_HEADERS = \
	catalogue_index.h \
	connect_dialogue.h \
	create_database_dialogue.h \
	database_connection.h \
	database_event_type.h \
	database_work.h \
	documents_notebook.h \
	execution_lexer.h \
	object_browser.h \
	object_browser_database_work_impl.h \
	object_browser_managed_work.h \
	object_browser_model.h \
	object_browser_scripts.h \
	object_browser_work.h \
	object_finder.h \
	object_model_reference.h \
	pg_error.h \
	pg_tools_registry.h \
	pqwx.h \
	pqwx_frame.h \
	pqwx_util.h \
	preferences_dialogue.h \
	results_notebook.h \
	script_editor.h \
	script_editor_pane.h \
	script_events.h \
	script_execution.h \
	script_query_work.h \
	script_query_work.h \
	server_connection.h \
	sql_dictionary.h \
	sql_logger.h \
	ssl_info.h \
	static_resources.h \
	work_launcher.h
SOURCES = $(PQWX_SOURCES) test_catalogue.cpp dump_catalogue.cpp
SQL_DICTIONARIES = object_browser.sql dependencies_view.sql object_browser_scripts.sql create_database_dialogue.sql
GENERATED_SOURCES = $(patsubst %.sql,%_sql.cpp,$(SQL_DICTIONARIES)) static_resources_txt.cpp script_editor_wordlists.cpp resources.cpp create_database_dialogue_encodings.cpp
PQWX_OBJS = $(PQWX_SOURCES:.cpp=.o) $(GENERATED_SOURCES:.cpp=.o)
ifneq (,$(findstring MINGW,$(host_system)))
PQWX_OBJS += pqwx_rc.o
PQWX_SOURCES += pqwx_rc.cpp
else
PQWX_OBJS += database_notification_monitor.o
PQWX_SOURCES += database_notification_monitor.cpp
endif
ifneq (,$(debversion))
PQWX_SOURCES += debian_pgcluster.cpp
PQWX_OBJS += debian_pgcluster.o
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
-include static_resources.d

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
	./format_sql_header -c ObjectBrowserSql -f 'ObjectBrowserWork::GetSqlDictionary' -h object_browser_work.h object_browser.sql $@

object_browser_scripts_sql.cpp: object_browser_scripts.sql format_sql_header
	./format_sql_header -c ObjectBrowserScriptsSql -f 'ScriptWork::GetSqlDictionary' -h object_browser_scripts.h object_browser_scripts.sql $@

dependencies_view_sql.cpp: dependencies_view.sql format_sql_header
	./format_sql_header -c DependenciesViewSql  -f 'DependenciesView::GetSqlDictionary' -h dependencies_view.h dependencies_view.sql $@

create_database_dialogue_sql.cpp: create_database_dialogue.sql format_sql_header
	./format_sql_header -c CreateDatabaseDialogueSql  -f 'CreateDatabaseDialogue::GetSqlDictionary' -h create_database_dialogue.h create_database_dialogue.sql $@

static_resources_txt.cpp: static_resources.txt static_resources.h
	./format_static_resources -f 'StaticResources::RegisterMemoryResources' -h static_resources.h -o $@ -d static_resources.d static_resources.txt

script_editor_wordlists.cpp: postgresql_wordlists_keywords.txt postgresql_wordlists_database_objects.txt postgresql_wordlists_sqlplus.txt postgresql_wordlists_user1.txt postgresql_wordlists_user2.txt postgresql_wordlists_user3.txt postgresql_wordlists_user4.txt ./format_wordlists
	./format_wordlists -p 'ScriptEditor::WordList' -o $@ keywords=postgresql_wordlists_keywords.txt \
		 database_objects=postgresql_wordlists_database_objects.txt \
		 sqlplus=postgresql_wordlists_sqlplus.txt \
		 user1=postgresql_wordlists_user1.txt \
		 user2=postgresql_wordlists_user2.txt \
		 user3=postgresql_wordlists_user3.txt \
		 user4=postgresql_wordlists_user4.txt

create_database_dialogue_encodings.cpp: encodings.txt format_encodings
	./format_encodings -h create_database_dialogue.h -f CreateDatabaseDialogue::GetPgEncodings encodings.txt $@

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
	rm -f *.o *.d $(EXECUTABLES) vcs_version.mk pqwx_version.h resources.h rc/*.c build_settings wx_flavour.h $(GENERATED_SOURCES)

.PHONY: FORCE
