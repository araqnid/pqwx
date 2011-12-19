-- SQL used by the object browser

-- The object browser extracts its results by position, so the order of columns is critical.

-- The introductory comments define a name and, optionally, a minimum
-- server version for that statement. Statments with the same name are
-- collected together, and the instance with the highest minimum
-- version that the connected server satisfies is taken. If there is
-- an instance with no version as well, that is used as a fallback.

-- SQL :: Server
SELECT version()

-- SQL :: Databases :: 8.2
SELECT pg_database.oid, datname, datistemplate, datallowconn,
       has_database_privilege(pg_database.oid, 'connect') AS can_connect,
       pg_shdescription.description
FROM pg_database
     LEFT JOIN pg_shdescription ON pg_shdescription.classoid = 'pg_database'::regclass
                                   AND pg_shdescription.objoid = pg_database.oid

-- SQL :: Databases
SELECT pg_database.oid, datname, datistemplate, datallowconn,
       true AS can_connect,
       null AS description
FROM pg_database

-- SQL :: Database Detail
SELECT owner.rolname, pg_encoding_to_char(encoding), datcollate, datctype, datconnlimit
FROM pg_database
     JOIN pg_roles owner ON owner.oid = pg_database.datdba
WHERE pg_database.oid = $1

-- SQL :: Roles :: 8.2
SELECT pg_roles.oid, rolname, rolcanlogin, rolsuper,
       pg_shdescription.description
FROM pg_roles
     LEFT JOIN pg_shdescription ON pg_shdescription.classoid = 'pg_authid'::regclass
                                   AND pg_shdescription.objoid = pg_roles.oid

-- SQL :: Roles :: 8.0
SELECT pg_roles.oid, rolname, rolcanlogin, rolsuper,
       null AS description
FROM pg_roles

-- SQL :: Roles
SELECT usesysid, usename, true, usesuper,
       null AS description
FROM pg_user
UNION ALL
SELECT grosysid, groname, false, false,
       null AS description
FROM pg_group

-- SQL :: Relations
SELECT pg_class.oid, nspname, relname, relkind,
       pg_description.description
FROM (SELECT oid, relname, relkind, relnamespace
      FROM pg_class
      WHERE relkind IN ('r','v')
            OR (relkind = 'S' AND NOT EXISTS (
                     -- exclude sequences that depend on an attribute (serial columns)
                     SELECT 1
                     FROM pg_depend
                     WHERE classid = 'pg_class'::regclass
                           AND objid = pg_class.oid
                           AND refclassid = 'pg_class'::regclass
                           AND refobjsubid > 0
                           AND deptype = 'a')
               )
     ) pg_class
     LEFT JOIN pg_description ON pg_description.classoid = 'pg_class'::regclass
                                 AND pg_description.objoid = pg_class.oid
                                 AND pg_description.objsubid = 0
     RIGHT JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace

-- SQL :: Table Detail

SELECT owner.rolname,
       pg_tablespace.spcname,
       relhasoids,
       relacl,
       reloptions
FROM pg_class
     JOIN pg_roles owner ON owner.oid = pg_class.relowner
     LEFT JOIN pg_tablespace ON pg_tablespace.oid = pg_class.reltablespace
WHERE pg_class.oid = $1

-- SQL :: Relation Column Detail

SELECT attname,
       pg_catalog.format_type(atttypid, atttypmod),
       attnotnull,
       atthasdef,
       pg_get_expr(pg_attrdef.adbin, pg_attrdef.adrelid),
       CASE WHEN attcollation <> typcollation THEN quote_ident(collname) END,
       attstattarget,
       CASE attstorage WHEN 'e' THEN 'EXTERNAL' END,
       attacl,
       attoptions
FROM pg_attribute
     LEFT JOIN pg_attrdef ON pg_attrdef.adrelid = pg_attribute.attrelid AND pg_attrdef.adnum = pg_attribute.attnum
     LEFT JOIN pg_collation ON pg_collation.oid = pg_attribute.attcollation
     JOIN pg_type ON pg_type.oid = pg_attribute.atttypid
WHERE pg_attribute.attrelid = $1
      AND NOT attisdropped
      AND attnum > 0
ORDER BY attnum

-- SQL :: View Detail

SELECT owner.rolname,
       pg_get_viewdef(pg_class.oid, true)
FROM pg_class
     JOIN pg_roles owner ON owner.oid = pg_class.relowner
WHERE pg_class.oid = $1

-- SQL :: Sequence Detail

-- SQL :: Function Detail

SELECT pg_get_function_arguments(pg_proc.oid),
       owner.rolname,
       prolang,
       CASE WHEN procost <> 100 THEN procost ELSE -1 END,
       CASE WHEN NOT proretset THEN -1 WHEN prorows <> 1000 THEN prorows ELSE -1 END,
       prosecdef,
       proisstrict,
       CASE provolatile WHEN 'i' THEN 'IMMUTABLE' WHEN 'v' THEN 'VOLATILE' WHEN 's' THEN 'STABLE' END,
       prosrc,
       format_type(prorettype, null),
       proretset,
       proconfig,
       proacl
FROM pg_proc
     JOIN pg_roles owner ON owner.oid = pg_proc.proowner
WHERE pg_proc.oid = $1

-- SQL :: Functions :: 8.4
SELECT pg_proc.oid, nspname, proname, pg_proc.oid::regprocedure,
       CASE WHEN proretset THEN 'fs'
            WHEN prorettype = 'trigger'::regtype THEN 'ft'
            WHEN proisagg THEN 'fa'
            WHEN proiswindow THEN 'fw'
            ELSE 'f' END,
       pg_description.description
FROM pg_proc
     JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace
     LEFT JOIN pg_description ON pg_description.classoid = 'pg_proc'::regclass
                                 AND pg_description.objoid = pg_proc.oid

-- SQL :: Functions
SELECT pg_proc.oid, nspname, proname, pg_proc.oid::regprocedure,
       CASE WHEN proretset THEN 'fs'
            WHEN prorettype = 'trigger'::regtype THEN 'ft'
            WHEN proisagg THEN 'fa'
            ELSE 'f' END,
       pg_description.description
FROM pg_proc
     JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace
     LEFT JOIN pg_description ON pg_description.classoid = 'pg_proc'::regclass
                                 AND pg_description.objoid = pg_proc.oid

-- SQL :: Columns
SELECT attname, pg_catalog.format_type(atttypid, atttypmod), NOT attnotnull, atthasdef,
       pg_description.description
FROM pg_attribute
     LEFT JOIN pg_description ON pg_description.classoid = 'pg_attribute'::regclass
                                 AND pg_description.objoid = pg_attribute.attrelid
                                 AND pg_description.objsubid = pg_attribute.attnum
WHERE attrelid = $1
      AND NOT attisdropped
      AND attnum > 0
ORDER BY attnum

-- SQL :: Indices :: 9.0
SELECT relname, indisunique, indisprimary, indisexclusion, indisclustered
FROM pg_index
     JOIN pg_class ON pg_class.oid = pg_index.indexrelid
WHERE indrelid = $1

-- SQL :: Indices
SELECT relname, indisunique, indisprimary, false AS indisexclusion, indisclustered
FROM pg_index
     JOIN pg_class ON pg_class.oid = pg_index.indexrelid
WHERE indrelid = $1

-- SQL :: Triggers :: 9.0
SELECT tgname, tgfoid::regprocedure, tgenabled
FROM pg_trigger
WHERE tgrelid = $1
      AND NOT tgisinternal

-- SQL :: Triggers :: 8.3
SELECT tgname, tgfoid::regprocedure, tgenabled
FROM pg_trigger
WHERE tgrelid = $1
      AND tgconstraint = 0

-- SQL :: Triggers
SELECT tgname, tgfoid::regprocedure, tgenabled
FROM pg_trigger
WHERE tgrelid = $1
      AND (NOT tgisconstraint
           OR NOT EXISTS (SELECT 1
               FROM pg_catalog.pg_depend d
                    JOIN pg_catalog.pg_constraint c ON (d.refclassid = c.tableoid AND d.refobjid = c.oid)
               WHERE d.classid = pg_trigger.tableoid
                     AND d.objid = pg_trigger.oid
                     AND d.deptype = 'i'
                     AND c.contype = 'f')
          )

-- SQL :: IndexSchema :: 9.1
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
                        pg_get_function_arguments(pg_proc.oid) AS objdisambig,
                        CASE WHEN prorettype = 'trigger'::regtype THEN 'ft' WHEN proretset THEN 'fs' WHEN proisagg THEN 'fa' WHEN proiswindow THEN 'fw' ELSE 'f' END AS objtype
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
                 -- extensions (9.1 onwards)
                 UNION ALL
                 SELECT extnamespace AS nspoid,
                        pg_extension.oid AS objid,
                        extname AS name,
                        null AS objdisambig,
			'x' AS objtype
                 FROM pg_extension
                 -- collations (9.1 onwards)
                 UNION ALL
                 SELECT collnamespace AS nspoid,
                        pg_collation.oid AS objid,
                        collname AS name,
                        null AS objdisambig,
			'O' AS objtype
                 FROM pg_collation
                ) x ON pg_namespace.oid = x.nspoid
WHERE NOT (nspname LIKE 'pg_%' AND nspname <> 'pg_catalog')
ORDER BY 1, 2, 3

-- SQL :: IndexSchema :: 8.4
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
                        pg_get_function_arguments(pg_proc.oid) AS objdisambig,
                        CASE WHEN prorettype = 'trigger'::regtype THEN 'ft' WHEN proretset THEN 'fs' WHEN proisagg THEN 'fa' WHEN proiswindow THEN 'fw' ELSE 'f' END AS objtype
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

-- SQL :: IndexSchema :: 8.3
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

-- SQL :: IndexSchema
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
                ) x ON pg_namespace.oid = x.nspoid
WHERE NOT (nspname LIKE 'pg_%' AND nspname <> 'pg_catalog')
ORDER BY 1, 2, 3
