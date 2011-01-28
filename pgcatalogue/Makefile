db_list := $(shell psql -Ppager=off -At -c "select datname from pg_database where has_database_privilege(oid, 'connect') and (not datistemplate or datname = 'template1') and datallowconn")

all: $(patsubst %,%.catalogue.txt,$(db_list))

%.catalogue.txt: catalogue_driver.sql
	psql -F'|' -At -Ppager=off -f catalogue_driver.sql -o $*.catalogue.txt $*

clean:
	-rm -f *.catalogue.txt
