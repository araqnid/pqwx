SELECT x.objid,
       objtype || CASE WHEN nspname LIKE 'pg_%' OR nspname = 'information_schema' THEN 'S'
                       ELSE '' END,
       nspname || '.' || objname,
       objdisambig
FROM pg_namespace
     INNER JOIN (-- tables, views, sequences
                 SELECT relnamespace AS nspoid,
                        pg_class.oid AS objid,
                        relname AS objname,
                        null AS objdisambig,
                        CASE relkind
                             WHEN 'r' THEN 't'
                             WHEN 'S' THEN 's'
                             WHEN 'v' THEN 'v'
                        END AS objtype
                 FROM pg_class
                 WHERE relkind IN ('r','v')
                       OR (relkind = 'S' AND NOT EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = pg_class.oid AND refclassid = 'pg_class'::regclass AND deptype = 'a'))
                 -- functions
                 UNION ALL
                 SELECT pronamespace AS nspoid,
                        pg_proc.oid AS objid,
                        proname AS objname,
                        pg_proc.oid::regprocedure::text AS objdisambig,
                        CASE WHEN prorettype = 'trigger'::regtype THEN 'ft' WHEN proretset THEN 'fs' WHEN proisagg THEN 'fa' ELSE 'f' END AS objtype
                 FROM pg_proc
		 WHERE -- exclude functions with 'internal'/'cstring' arguments
                       NOT (proargtypes::text <> '' AND EXISTS (SELECT 1 FROM regexp_split_to_table(proargtypes::text, ' ') x(typeid) WHERE typeid::oid IN ('internal'::regtype, 'cstring'::regtype)))
                       -- exclude functions with 'internal'/'cstring' return type
                       AND NOT prorettype IN ('internal'::regtype, 'cstring'::regtype)
                 -- types
                 UNION ALL
                 SELECT typnamespace AS nspoid,
                        pg_type.oid AS objid,
                        typname AS objname,
                        null AS objdisambig,
                        'T' AS objtype
                 FROM pg_type
                 WHERE -- exclude array types (create type "foo" implicity creates type "foo[]")
                       NOT (typname ~ '^_' AND typelem > 0 AND EXISTS (SELECT 1 FROM pg_type eltype WHERE eltype.oid = pg_type.typelem AND eltype.typarray = pg_type.oid))
                       -- exclude types that embody relations
                       AND NOT typrelid > 0
                ) x ON pg_namespace.oid = x.nspoid
WHERE NOT (nspname LIKE 'pg_%' AND nspname <> 'pg_catalog')
ORDER BY 1, 2, 3
;
