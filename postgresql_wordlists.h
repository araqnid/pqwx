// -*- mode: c++ -*-

#ifndef __postgresql_wordlists_h
#define __postgresql_wordlists_h

static wxString postgresql_wordlist_keywords = 
  _T("absolute action add admin after aggregate \
alias all allocate alter and any are array as asc \
assertion at authorization \
before begin binary bit blob body boolean both breadth by \
call cascade cascaded case cast catalog char character \
check class clob close collate collation column commit \
completion connect connection constraint constraints \
constructor continue corresponding create cross cube current \
current_date current_path current_role current_time current_timestamp \
current_user cursor cycle \
data date day deallocate dec decimal declare default \
deferrable deferred delete depth deref desc describe descriptor \
destroy destructor deterministic dictionary diagnostics disconnect \
distinct domain double drop dynamic \
each else end end-exec equals escape every except \
exception exec execute exists exit external \
false fetch first float for foreign found from free full \
function \
general get global go goto grant group grouping \
having host hour \
identity if ignore immediate in indicator initialize initially \
inner inout input insert int integer intersect interval \
into is isolation iterate \
join \
key \
language large last lateral leading left less level like \
limit local localtime localtimestamp locator \
map match minute modifies modify module month \
names national natural nchar nclob new next no none \
not null numeric \
object of off old on only open operation option \
or order ordinality out outer output \
package pad parameter parameters partial path postfix precision prefix \
preorder prepare preserve primary \
prior privileges procedure public \
read reads real recursive ref references referencing relative \
restrict result return returns revoke right \
role rollback rollup routine row rows \
savepoint schema scroll scope search second section select \
sequence session session_user set sets size smallint some| space \
specific specifictype sql sqlexception sqlstate sqlwarning start \
state statement static structure system_user \
table temporary terminate than then time timestamp \
timezone_hour timezone_minute to trailing transaction translation \
treat trigger true \
under union unique unknown \
unnest update usage user using \
value values varchar variable varying view \
when whenever where with without work write \
year \
zone");

static wxString postgresql_wordlist_database_objects = 
  _T("all alter and any array as asc at authid avg begin between \
binary_integer \
body boolean bulk by char char_base check close cluster collect \
comment commit compress connect constant create current currval \
cursor date day declare decimal default delete desc distinct \
do drop else elsif end exception exclusive execute exists exit \
extends false fetch float for forall from function goto group \
having heap hour if immediate in index indicator insert integer \
interface intersect interval into is isolation java level like \
limited lock long loop max min minus minute mlslabel mod mode \
month natural naturaln new nextval nocopy not nowait null number \
number_base ocirowid of on opaque open operator option or order \
organization others out package partition pctfree pls_integer \
positive positiven pragma prior private procedure public raise \
range raw real record ref release return reverse rollback row \
rowid rownum rowtype savepoint second select separate set share \
smallint space sql sqlcode sqlerrm start stddev subtype successful \
sum synonym sysdate table then time timestamp to trigger true \
type uid union unique update use user validate values varchar \
varchar2 variance view when whenever where while with work write \
year zone");

static wxString postgresql_wordlist_sqlplus =
	wxEmptyString;

static wxString postgresql_wordlist_user1 =
	wxEmptyString;

static wxString postgresql_wordlist_user2 =
	wxEmptyString;

static wxString postgresql_wordlist_user3 =
	wxEmptyString;

static wxString postgresql_wordlist_user4 =
	wxEmptyString;

#endif
