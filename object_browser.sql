-- SQL used by the object browser
-- The object browser extracts its results by position, so the order of columns is critical

-- SQL :: Server
SELECT version()

-- SQL :: Databases
SELECT pg_database.oid, datname, datistemplate, datallowconn,
       has_database_privilege(pg_database.oid, 'connect') AS can_connect,
       pg_shdescription.description
FROM pg_database
     LEFT JOIN pg_shdescription ON pg_shdescription.classoid = 'pg_database'::regclass
                                   AND pg_shdescription.objoid = pg_database.oid

-- SQL :: Roles
SELECT pg_roles.oid, rolname, rolcanlogin, rolsuper,
       pg_shdescription.description
FROM pg_roles
     LEFT JOIN pg_shdescription ON pg_shdescription.classoid = 'pg_authid'::regclass
                                   AND pg_shdescription.objoid = pg_roles.oid

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

-- SQL :: Functions
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

-- SQL :: Functions83
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
