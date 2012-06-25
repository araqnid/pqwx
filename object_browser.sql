-- SQL used by the object browser

-- The object browser extracts its results by position, so the order of columns is critical.

-- The introductory comments define a name and, optionally, a minimum
-- server version for that statement. Statments with the same name are
-- collected together, and the instance with the highest minimum
-- version that the connected server satisfies is taken. If there is
-- an instance with no version as well, that is used as a fallback.

-- SQL :: SetupObjectBrowserConnection
SET search_path = pg_catalog

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

-- SQL :: Tablespaces :: 9.2
SELECT pg_tablespace.oid, spcname, pg_catalog.pg_tablespace_location(pg_tablespace.oid)
FROM pg_tablespace

-- SQL :: Tablespaces
SELECT pg_tablespace.oid, spcname, spclocation
FROM pg_tablespace

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

-- SQL :: Relations :: 9.1
SELECT pg_class.oid, nspname, relname, relkind,
       pg_extension.extname, relpersistence = 'u' AS is_unlogged
FROM (SELECT oid, relname, relkind, relnamespace, relpersistence
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
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_class'::regclass
                         AND pg_depend.objid = pg_class.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
     LEFT JOIN pg_extension ON pg_extension.oid = pg_depend.refobjid
     RIGHT JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace

-- SQL :: Relations
SELECT pg_class.oid, nspname, relname, relkind,
       NULL AS extname, false AS is_unlogged
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
     RIGHT JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace

-- SQL :: Functions :: 9.1
SELECT pg_proc.oid, nspname, proname, pg_get_function_arguments(pg_proc.oid),
       CASE WHEN proretset THEN 'fs'
            WHEN prorettype = 'trigger'::regtype THEN 'ft'
            WHEN proisagg THEN 'fa'
            WHEN proiswindow THEN 'fw'
            ELSE 'f' END,
       extname
FROM pg_proc
     JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_proc'::regclass
                         AND pg_depend.objid = pg_proc.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
     LEFT JOIN pg_extension ON pg_extension.oid = pg_depend.refobjid

-- SQL :: Functions :: 8.4
SELECT pg_proc.oid, nspname, proname, pg_get_function_arguments(pg_proc.oid),
       CASE WHEN proretset THEN 'fs'
            WHEN prorettype = 'trigger'::regtype THEN 'ft'
            WHEN proisagg THEN 'fa'
            WHEN proiswindow THEN 'fw'
            ELSE 'f' END,
       NULL AS extname
FROM pg_proc
     JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace

-- SQL :: Functions
SELECT oid, nspname, proname, substr(longname, position('(' in longname) + 1, length(longname) - position('(' in longname) - 1),
       type, NULL as extname
FROM (
SELECT pg_proc.oid, nspname, proname, pg_proc.oid::regprocedure::text as longname,
       CASE WHEN proretset THEN 'fs'
            WHEN prorettype = 'trigger'::regtype THEN 'ft'
            WHEN proisagg THEN 'fa'
            ELSE 'f' END AS type
FROM pg_proc
     JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace
) x

-- SQL :: Object Descriptions
SELECT objoid, description
FROM pg_description
WHERE classoid IN ('pg_class'::regclass, 'pg_proc'::regclass)
AND objsubid = 0

-- SQL :: Columns
SELECT attname, pg_catalog.format_type(atttypid, atttypmod), NOT attnotnull, atthasdef,
       pg_description.description, attnum
FROM pg_attribute
     LEFT JOIN pg_description ON pg_description.classoid = 'pg_attribute'::regclass
                                 AND pg_description.objoid = pg_attribute.attrelid
                                 AND pg_description.objsubid = pg_attribute.attnum
WHERE attrelid = $1
      AND NOT attisdropped
      AND attnum > 0
ORDER BY attnum

-- SQL :: Indices :: 9.0
SELECT relname, indisunique, indisprimary, indisexclusion, indisclustered, indkey, pg_get_indexdef(indexrelid, attnum, true) as indexattdef
FROM pg_index
     JOIN pg_class ON pg_class.oid = pg_index.indexrelid
     JOIN pg_attribute ON pg_attribute.attrelid = pg_index.indexrelid
WHERE indrelid = $1
ORDER BY relname, pg_attribute.attnum

-- SQL :: Indices
SELECT relname, indisunique, indisprimary, false as indisexclusion, indisclustered, indkey, pg_get_indexdef(indexrelid, attnum, true) as indexattdef
FROM pg_index
     JOIN pg_class ON pg_class.oid = pg_index.indexrelid
     JOIN pg_attribute ON pg_attribute.attrelid = pg_index.indexrelid
WHERE indrelid = $1
ORDER BY relname, pg_attribute.attnum

-- SQL :: Constraints
SELECT conname, contype, consrc
FROM pg_constraint
WHERE conrelid = $1

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

-- SQL :: Sequences
SELECT seq.oid, nspname, seq.relname, pg_depend.refobjsubid
FROM pg_class seq
     JOIN pg_namespace ON pg_namespace.oid = seq.relnamespace
     JOIN pg_depend ON pg_depend.classid = 'pg_class'::regclass
                       AND pg_depend.objid = seq.oid
                       AND pg_depend.deptype = 'a'
                       AND pg_depend.refclassid = 'pg_class'::regclass
                       AND pg_depend.refobjsubid > 0
WHERE pg_depend.refobjid = $1
      AND seq.relkind = 'S'

-- SQL :: Text search dictionaries :: 9.1
SELECT pg_ts_dict.oid, nspname, dictname, extname
FROM pg_ts_dict
     JOIN pg_namespace ON pg_namespace.oid = pg_ts_dict.dictnamespace
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_ts_dict'::regclass
                         AND pg_depend.objid = pg_ts_dict.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
     LEFT JOIN pg_extension ON pg_extension.oid = pg_depend.refobjid

-- SQL :: Text search dictionaries
SELECT pg_ts_dict.oid, nspname, dictname, null AS extname
FROM pg_ts_dict
     JOIN pg_namespace ON pg_namespace.oid = pg_ts_dict.dictnamespace

-- SQL :: Text search parsers :: 9.1
SELECT pg_ts_parser.oid, nspname, prsname, extname
FROM pg_ts_parser
     JOIN pg_namespace ON pg_namespace.oid = pg_ts_parser.prsnamespace
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_ts_parser'::regclass
                         AND pg_depend.objid = pg_ts_parser.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
     LEFT JOIN pg_extension ON pg_extension.oid = pg_depend.refobjid

-- SQL :: Text search parsers
SELECT pg_ts_parser.oid, nspname, prsname, null AS extname
FROM pg_ts_parser
     JOIN pg_namespace ON pg_namespace.oid = pg_ts_parser.prsnamespace

-- SQL :: Text search templates :: 9.1
SELECT pg_ts_template.oid, nspname, tmplname, extname
FROM pg_ts_template
     JOIN pg_namespace ON pg_namespace.oid = pg_ts_template.tmplnamespace
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_ts_template'::regclass
                         AND pg_depend.objid = pg_ts_template.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
     LEFT JOIN pg_extension ON pg_extension.oid = pg_depend.refobjid

-- SQL :: Text search templates
SELECT pg_ts_template.oid, nspname, tmplname, null AS extname
FROM pg_ts_template
     JOIN pg_namespace ON pg_namespace.oid = pg_ts_template.tmplnamespace

-- SQL :: Text search configurations :: 9.1
SELECT pg_ts_config.oid, nspname, cfgname, null AS extname
FROM pg_ts_config
     JOIN pg_namespace ON pg_namespace.oid = pg_ts_config.cfgnamespace
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_ts_config'::regclass
                         AND pg_depend.objid = pg_ts_config.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
     LEFT JOIN pg_extension ON pg_extension.oid = pg_depend.refobjid

-- SQL :: Text search configurations
SELECT pg_ts_config.oid, nspname, cfgname, null AS extname
FROM pg_ts_config
     JOIN pg_namespace ON pg_namespace.oid = pg_ts_config.cfgnamespace

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
                             WHEN 'r' THEN CASE relpersistence WHEN 'u' THEN 'tu' ELSE 't' END
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
                 -- text search dictionaries
                 UNION ALL
                 SELECT dictnamespace AS nspoid,
                        pg_ts_dict.oid AS objid,
                        dictname AS objname,
                        null AS objdisambig,
                        'Fd' AS objtype
                 FROM pg_ts_dict
                 -- text search parsers
                 UNION ALL
                 SELECT prsnamespace AS nspoid,
                        pg_ts_parser.oid AS objid,
                        prsname AS objname,
                        null AS objdisambig,
                        'Fp' AS objtype
                 FROM pg_ts_parser
                 -- text search templates
                 UNION ALL
                 SELECT tmplnamespace AS nspoid,
                        pg_ts_template.oid AS objid,
                        tmplname AS objname,
                        null AS objdisambig,
                        'Ft' AS objtype
                 FROM pg_ts_template
                 -- text search configurations
                 UNION ALL
                 SELECT cfgnamespace AS nspoid,
                        pg_ts_config.oid AS objid,
                        cfgname AS objname,
                        null AS objdisambig,
                        'F' AS objtype
                 FROM pg_ts_config
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
                 -- text search dictionaries
                 UNION ALL
                 SELECT dictnamespace AS nspoid,
                        pg_ts_dict.oid AS objid,
                        dictname AS objname,
                        null AS objdisambig,
                        'Fd' AS objtype
                 FROM pg_ts_dict
                 -- text search parsers
                 UNION ALL
                 SELECT prsnamespace AS nspoid,
                        pg_ts_parser.oid AS objid,
                        prsname AS objname,
                        null AS objdisambig,
                        'Fp' AS objtype
                 FROM pg_ts_parser
                 -- text search templates
                 UNION ALL
                 SELECT tmplnamespace AS nspoid,
                        pg_ts_template.oid AS objid,
                        tmplname AS objname,
                        null AS objdisambig,
                        'Ft' AS objtype
                 FROM pg_ts_template
                 -- text search configurations
                 UNION ALL
                 SELECT cfgnamespace AS nspoid,
                        pg_ts_config.oid AS objid,
                        cfgname AS objname,
                        null AS objdisambig,
                        'F' AS objtype
                 FROM pg_ts_config
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
                        substr(pg_proc.oid::regprocedure::text, position('(' in pg_proc.oid::regprocedure::text) + 1, length(pg_proc.oid::regprocedure::text) - position('(' in pg_proc.oid::regprocedure::text) - 1) AS objdisambig,
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
