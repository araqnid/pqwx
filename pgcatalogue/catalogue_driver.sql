SELECT x.objid, objtype, nspname || '.' || objname
FROM pg_namespace
     INNER JOIN (SELECT relnamespace AS nspoid, pg_class.oid as objid, relname as objname, relkind as objtype FROM pg_class WHERE relkind IN ('r','v') OR (relkind = 'S' AND NOT EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = pg_class.oid AND refclassid = 'pg_class'::regclass AND deptype = 'a'))
                UNION ALL SELECT pronamespace AS nspoid, pg_proc.oid, proname, case when prorettype = 'trigger'::regtype then 'ft' when proretset then 'fs' when proisagg then 'fa' when proiswindow then 'fw' else 'f' end FROM pg_proc) x ON pg_namespace.oid = x.nspoid WHERE (nspname NOT LIKE 'pg_%' OR nspname = 'pg_catalog')
ORDER BY 1, 2, 3
;

-- select pg_class.oid, case relkind when 'r' then 'TABLE' when 'v' then 'VIEW' end, nspname || '.' || relname
-- from pg_class JOIN pg_namespace on pg_namespace.oid = pg_class.relnamespace 
-- where relkind in ('r', 'v') and (nspname not like 'pg_%' /* or nspname = 'pg_catalog' */) and nspname <> 'information_schema';

-- select pg_class.oid, 'SEQUENCE', nspname || '.' || relname
-- from pg_class JOIN pg_namespace on pg_namespace.oid = pg_class.relnamespace 
-- where relkind = 'S' and (nspname not like 'pg_%' /* or nspname = 'pg_catalog' */) and nspname <> 'information_schema'
-- and not exists (select 1 from pg_depend where classid = 'pg_class'::regclass and objid = pg_class.oid and refclassid = 'pg_class'::regclass and deptype = 'a')
-- ;

-- select pg_proc.oid, 'FUNC', nspname || '.' || proname
-- from pg_proc join pg_namespace on pg_namespace.oid = pg_proc.pronamespace
-- where (nspname not like 'pg_%' /* or nspname = 'pg_catalog' */) and nspname <> 'information_schema';
