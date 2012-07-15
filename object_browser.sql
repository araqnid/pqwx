-- SQL used by the object browser

-- The object browser extracts its results by position, so the order of columns is critical.

-- The introductory comments define a name and, optionally, a minimum
-- server version for that statement. Statments with the same name are
-- collected together, and the instance with the highest minimum
-- version that the connected server satisfies is taken. If there is
-- an instance with no version as well, that is used as a fallback.

-- SQL :: SetupObjectBrowserConnection
SET search_path = pg_catalog

-- SQL :: Role
SELECT rolcreatedb, rolcreaterole, rolsuper, rolname
FROM pg_roles
WHERE rolname = current_user

-- SQL :: Databases :: 8.2
SELECT pg_database.oid, datname, datistemplate, datallowconn,
       has_database_privilege(pg_database.oid, 'connect') AS can_connect,
       pg_shdescription.description,
       owner.rolname
FROM pg_database
     JOIN pg_roles owner ON owner.oid = pg_database.datdba
     LEFT JOIN pg_shdescription ON pg_shdescription.classoid = 'pg_database'::regclass
                                   AND pg_shdescription.objoid = pg_database.oid

-- SQL :: Databases
SELECT pg_database.oid, datname, datistemplate, datallowconn,
       true AS can_connect,
       null AS description,
       owner.rolname
FROM pg_database
     JOIN pg_roles owner ON owner.oid = pg_database.datdba

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

-- SQL :: Schemas
SELECT pg_namespace.oid, nspname,
       has_schema_privilege(pg_namespace.oid, 'USAGE') AS accessible
FROM pg_namespace

-- SQL :: Extensions :: 9.1
SELECT pg_extension.oid, extname
FROM pg_extension

-- SQL :: Extensions
SELECT NULL::oid, NULL::text
WHERE false

-- SQL :: Relations :: 9.1
SELECT relnamespace,
       pg_depend.refobjid AS extoid,
       pg_class.oid, relname, relkind,
       relpersistence = 'u' AS is_unlogged
FROM (SELECT oid, relname, relkind, relnamespace, relpersistence
      FROM pg_class
      WHERE (relkind IN ('r','v')
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
            ) AND has_schema_privilege(relnamespace, 'USAGE')
     ) pg_class
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_class'::regclass
                         AND pg_depend.objid = pg_class.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass

-- SQL :: Relations
SELECT relnamespace,
       NULL,
       pg_class.oid, relname, relkind,
       false AS is_unlogged,
       has_schema_privilege(relnamespace, 'USAGE')
FROM (SELECT oid, relname, relkind, relnamespace
      FROM pg_class
      WHERE (relkind IN ('r','v')
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
            ) AND has_schema_privilege(relnamespace, 'USAGE')
     ) pg_class

-- SQL :: Functions :: 9.1
SELECT pronamespace,
       pg_depend.refobjid AS extoid,
       pg_proc.oid, proname, pg_get_function_arguments(pg_proc.oid),
       CASE WHEN proretset THEN 'fs'
            WHEN prorettype = 'trigger'::regtype THEN 'ft'
            WHEN proisagg THEN 'fa'
            WHEN proiswindow THEN 'fw'
            ELSE 'f' END
FROM pg_proc
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_proc'::regclass
                         AND pg_depend.objid = pg_proc.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
WHERE has_schema_privilege(pronamespace, 'USAGE')

-- SQL :: Functions :: 8.4
SELECT pronamespace,
       NULL,
       pg_proc.oid, proname, pg_get_function_arguments(pg_proc.oid),
       CASE WHEN proretset THEN 'fs'
            WHEN prorettype = 'trigger'::regtype THEN 'ft'
            WHEN proisagg THEN 'fa'
            WHEN proiswindow THEN 'fw'
            ELSE 'f' END
FROM pg_proc
WHERE has_schema_privilege(pronamespace, 'USAGE')

-- SQL :: Functions
SELECT nspoid,
       NULL,
       oid, proname, substr(longname, position('(' in longname) + 1, length(longname) - position('(' in longname) - 1),
       type
FROM (
SELECT pg_proc.oid, pg_namespace.oid as nspoid, nspname, proname, pg_proc.oid::regprocedure::text as longname,
       CASE WHEN proretset THEN 'fs'
            WHEN prorettype = 'trigger'::regtype THEN 'ft'
            WHEN proisagg THEN 'fa'
            ELSE 'f' END AS type
FROM pg_proc
WHERE has_schema_privilege(pronamespace, 'USAGE')
) x

-- SQL :: Object Descriptions
SELECT objoid, description
FROM pg_description
WHERE classoid IN ('pg_class'::regclass, 'pg_proc'::regclass, 'pg_ts_config'::regclass, 'pg_ts_parser'::regclass, 'pg_ts_template'::regclass, 'pg_ts_config'::regclass)
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
      AND contype = 'c'

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

-- SQL :: Sequences :: 9.1
SELECT relnamespace,
       pg_depend_ext.refobjid AS extoid,
       seq.oid, seq.relname, pg_depend.refobjsubid
FROM pg_class seq
     JOIN pg_depend ON pg_depend.classid = 'pg_class'::regclass
                       AND pg_depend.objid = seq.oid
                       AND pg_depend.deptype = 'a'
                       AND pg_depend.refclassid = 'pg_class'::regclass
                       AND pg_depend.refobjsubid > 0
     LEFT JOIN pg_depend pg_depend_ext ON pg_depend_ext.classid = 'pg_class'::regclass
                         AND pg_depend_ext.objid = seq.oid
                         AND pg_depend_ext.refclassid = 'pg_extension'::regclass
WHERE pg_depend.refobjid = $1
      AND seq.relkind = 'S'
      AND has_schema_privilege(relnamespace, 'USAGE')

-- SQL :: Sequences
SELECT pg_namespace.oid,
       NULL,
       seq.oid, seq.relname, pg_depend.refobjsubid
FROM pg_class seq
     JOIN pg_depend ON pg_depend.classid = 'pg_class'::regclass
                       AND pg_depend.objid = seq.oid
                       AND pg_depend.deptype = 'a'
                       AND pg_depend.refclassid = 'pg_class'::regclass
                       AND pg_depend.refobjsubid > 0
WHERE pg_depend.refobjid = $1
      AND seq.relkind = 'S'
      AND has_schema_privilege(relnamespace, 'USAGE')

-- SQL :: Text search dictionaries :: 9.1
SELECT dictnamespace,
       pg_depend.refobjid AS extoid,
       pg_ts_dict.oid, dictname
FROM pg_ts_dict
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_ts_dict'::regclass
                         AND pg_depend.objid = pg_ts_dict.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
WHERE has_schema_privilege(dictnamespace, 'USAGE')

-- SQL :: Text search dictionaries
SELECT dictnamespace,
       NULL,
       pg_ts_dict.oid, dictname
FROM pg_ts_dict
WHERE has_schema_privilege(dictnamespace, 'USAGE')

-- SQL :: Text search parsers :: 9.1
SELECT prsnamespace,
       pg_depend.refobjid AS extoid,
       pg_ts_parser.oid, prsname
FROM pg_ts_parser
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_ts_parser'::regclass
                         AND pg_depend.objid = pg_ts_parser.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
WHERE has_schema_privilege(prsnamespace, 'USAGE')

-- SQL :: Text search parsers
SELECT prsnamespace,
       NULL,
       pg_ts_parser.oid, prsname
FROM pg_ts_parser
WHERE has_schema_privilege(prsnamespace, 'USAGE')

-- SQL :: Text search templates :: 9.1
SELECT tmplnamespace,
       pg_depend.refobjid AS extoid,
       pg_ts_template.oid, tmplname
FROM pg_ts_template
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_ts_template'::regclass
                         AND pg_depend.objid = pg_ts_template.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
WHERE has_schema_privilege(tmplnamespace, 'USAGE')

-- SQL :: Text search templates
SELECT tmplnamespace,
       NULL,
       pg_ts_template.oid, tmplname
FROM pg_ts_template
WHERE has_schema_privilege(tmplnamespace, 'USAGE')

-- SQL :: Text search configurations :: 9.1
SELECT cfgnamespace,
       pg_depend.refobjid AS extoid,
       pg_ts_config.oid, cfgname
FROM pg_ts_config
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_ts_config'::regclass
                         AND pg_depend.objid = pg_ts_config.oid
                         AND pg_depend.refclassid = 'pg_extension'::regclass
WHERE has_schema_privilege(cfgnamespace, 'USAGE')

-- SQL :: Text search configurations
SELECT cfgnamespace,
       NULL,
       pg_ts_config.oid, cfgname
FROM pg_ts_config
WHERE has_schema_privilege(cfgnamespace, 'USAGE')

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
                 WHERE (relkind IN ('r','v')
                       OR (relkind = 'S' AND NOT EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = pg_class.oid AND refclassid = 'pg_class'::regclass AND deptype = 'a')))
                       AND has_schema_privilege(relnamespace, 'USAGE')
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
                       AND has_schema_privilege(pronamespace, 'USAGE')
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
                       AND has_schema_privilege(typnamespace, 'USAGE')
                 -- extensions (9.1 onwards)
                 UNION ALL
                 SELECT extnamespace AS nspoid,
                        pg_extension.oid AS objid,
                        extname AS name,
                        null AS objdisambig,
			'x' AS objtype
                 FROM pg_extension
                 WHERE has_schema_privilege(extnamespace, 'USAGE')
                 -- collations (9.1 onwards)
                 UNION ALL
                 SELECT collnamespace AS nspoid,
                        pg_collation.oid AS objid,
                        collname AS name,
                        null AS objdisambig,
			'O' AS objtype
                 FROM pg_collation
                 WHERE has_schema_privilege(collnamespace, 'USAGE')
                 -- text search dictionaries
                 UNION ALL
                 SELECT dictnamespace AS nspoid,
                        pg_ts_dict.oid AS objid,
                        dictname AS objname,
                        null AS objdisambig,
                        'Fd' AS objtype
                 FROM pg_ts_dict
                 WHERE has_schema_privilege(dictnamespace, 'USAGE')
                 -- text search parsers
                 UNION ALL
                 SELECT prsnamespace AS nspoid,
                        pg_ts_parser.oid AS objid,
                        prsname AS objname,
                        null AS objdisambig,
                        'Fp' AS objtype
                 FROM pg_ts_parser
                 WHERE has_schema_privilege(prsnamespace, 'USAGE')
                 -- text search templates
                 UNION ALL
                 SELECT tmplnamespace AS nspoid,
                        pg_ts_template.oid AS objid,
                        tmplname AS objname,
                        null AS objdisambig,
                        'Ft' AS objtype
                 FROM pg_ts_template
                 WHERE has_schema_privilege(tmplnamespace, 'USAGE')
                 -- text search configurations
                 UNION ALL
                 SELECT cfgnamespace AS nspoid,
                        pg_ts_config.oid AS objid,
                        cfgname AS objname,
                        null AS objdisambig,
                        'F' AS objtype
                 FROM pg_ts_config
                 WHERE has_schema_privilege(cfgnamespace, 'USAGE')
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
                 WHERE (relkind IN ('r','v')
                       OR (relkind = 'S' AND NOT EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = pg_class.oid AND refclassid = 'pg_class'::regclass AND deptype = 'a')))
                       AND has_schema_privilege(relnamespace, 'USAGE')
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
		       AND has_schema_privilege(pronamespace, 'USAGE')
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
		       AND has_schema_privilege(typnamespace, 'USAGE')
                 -- text search dictionaries
                 UNION ALL
                 SELECT dictnamespace AS nspoid,
                        pg_ts_dict.oid AS objid,
                        dictname AS objname,
                        null AS objdisambig,
                        'Fd' AS objtype
                 FROM pg_ts_dict
		 WHERE has_schema_privilege(dictnamespace, 'USAGE')
                 -- text search parsers
                 UNION ALL
                 SELECT prsnamespace AS nspoid,
                        pg_ts_parser.oid AS objid,
                        prsname AS objname,
                        null AS objdisambig,
                        'Fp' AS objtype
                 FROM pg_ts_parser
		 WHERE has_schema_privilege(prsnamespace, 'USAGE')
                 -- text search templates
                 UNION ALL
                 SELECT tmplnamespace AS nspoid,
                        pg_ts_template.oid AS objid,
                        tmplname AS objname,
                        null AS objdisambig,
                        'Ft' AS objtype
                 FROM pg_ts_template
		 WHERE has_schema_privilege(tmplnamespace, 'USAGE')
                 -- text search configurations
                 UNION ALL
                 SELECT cfgnamespace AS nspoid,
                        pg_ts_config.oid AS objid,
                        cfgname AS objname,
                        null AS objdisambig,
                        'F' AS objtype
                 FROM pg_ts_config
		 WHERE has_schema_privilege(cfgnamespace, 'USAGE')
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
                 WHERE (relkind IN ('r','v')
                       OR (relkind = 'S' AND NOT EXISTS (SELECT 1 FROM pg_depend WHERE classid = 'pg_class'::regclass AND objid = pg_class.oid AND refclassid = 'pg_class'::regclass AND deptype = 'a')))
                       AND has_schema_privilege(relnamespace, 'USAGE')
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
                       AND has_schema_privilege(pronamespace, 'USAGE')
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
                       AND has_schema_privilege(typnamespace, 'USAGE')
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
