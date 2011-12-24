-- SQL used by the dependencies view

-- SQL :: Dependents
SELECT deptype, refclassid, refobjid, refobjsubid, refobjsubid,
       CASE WHEN refobjsubid = 0
            THEN CASE refclassid
                 WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace WHERE pg_class.oid = pg_depend.refobjid)
                 WHEN 'pg_type'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(typname) FROM pg_type JOIN pg_namespace ON pg_namespace.oid = pg_type.typnamespace WHERE pg_type.oid = pg_depend.refobjid)
		 WHEN 'pg_namespace'::regclass
		 THEN (SELECT quote_ident(nspname) || '.' FROM pg_namespace WHERE pg_namespace.oid = pg_depend.refobjid)
		 WHEN 'pg_proc'::regclass
		 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(proname) || '(' || pg_get_function_arguments(pg_proc.oid) || ')' FROM pg_proc JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace WHERE pg_proc.oid = pg_depend.refobjid)
                 ELSE refclassid::regclass::text || '#' || refobjid
                 END
            ELSE refclassid::regclass::text || '#' || refobjid || '#' || refobjsubid::text
       END
FROM pg_depend
WHERE pg_depend.classid = $1 AND pg_depend.objid = $2 AND pg_depend.objsubid = $3

-- SQL :: Dependencies
SELECT deptype, classid, objid, objsubid, refobjsubid,
       CASE WHEN objsubid = 0
            THEN CASE classid
                 WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace WHERE pg_class.oid = pg_depend.objid)
                 WHEN 'pg_type'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(typname) FROM pg_type JOIN pg_namespace ON pg_namespace.oid = pg_type.typnamespace WHERE pg_type.oid = pg_depend.objid)
                 ELSE classid::regclass::text || '#' || objid
                 END
            ELSE classid::regclass::text || '#' || objid || '#' || objsubid::text
       END
FROM pg_depend
WHERE pg_depend.refclassid = $1 AND pg_depend.refobjid = $2 AND pg_depend.refobjsubid = $3
