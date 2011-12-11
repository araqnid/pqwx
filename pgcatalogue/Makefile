PG_DRIVER=postgresql_catalogue_driver.sql
MSSQL_DRIVER=mssql_catalogue_driver.sql
#MSSQL_SERVER=
#MSSQL_USER=
#MSSQL_PASSWORD=

PG_DBLIST=postgres template1
#PG_DBLIST=$(shell psql --set ON_ERROR_STOP=true -Ppager=off -At -c "select datname from pg_database where has_database_privilege(oid, 'connect') and (not datistemplate or datname = 'template1') and datallowconn")

#MSSQL_DBLIST=$(shell ( echo "select name from sys.databases where name not like 'spt_%'" && echo '\go' ) | sqsh -m bcp -D master -S '$(MSSQL_SERVER)' -U '$(MSSQL_USER)' -P '$(MSSQL_PASSWORD)' | sed -e 's/|.*//' )
MSSQL_DBLIST=

.PHONY: all postgresql mssql

all: postgresql mssql

postgresql: $(patsubst %,%.pg.cat,$(PG_DBLIST))

mssql: $(patsubst %,%.mssql.cat,$(MSSQL_DBLIST))

%.pg.cat: $(PG_DRIVER)
	psql --set ON_ERROR_STOP=true -F'|' -At -f $(PG_DRIVER) -o $*.pg.cat $*

%.mssql.cat: $(MSSQL_DRIVER)
	sqsh < $(MSSQL_DRIVER) -m bcp -D $* -S '$(MSSQL_SERVER)' -U '$(MSSQL_USER)' -P '$(MSSQL_PASSWORD)' -o $@

clean:
	-rm -f *.cat
