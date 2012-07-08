-- SQL used by the create database dialogue

-- SQL :: List users
SELECT rolname
FROM pg_roles
ORDER BY 1

-- SQL :: List templates
SELECT datname
FROM pg_database
WHERE pg_database.datistemplate
ORDER BY CASE datname WHEN 'template1' THEN 0 WHEN 'template0' THEN 1 ELSE 2 END,
         datname

-- SQL :: List collations
SELECT nspname, collname,
       pg_encoding_to_char(collencoding),
       collcollate, collctype
FROM pg_collation
     JOIN pg_namespace ON pg_namespace.oid = pg_collation.collnamespace
WHERE pg_collation.oid <> 100 -- omit 'default'
      AND pg_collation_is_visible(pg_collation.oid)
ORDER BY CASE WHEN nspname = 'pg_catalog' THEN 1 ELSE 0 END, nspname, collname
