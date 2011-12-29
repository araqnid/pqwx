-- SQL used by the dependencies view

-- SQL :: Dependents :: 8.4
SELECT deptype, refclassid, refobjid, refobjsubid, refobjsubid,
       CASE WHEN refobjsubid = 0
            THEN CASE refclassid
                 WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace WHERE pg_class.oid = pg_depend.refobjid)
                 WHEN 'pg_type'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(typname) FROM pg_type JOIN pg_namespace ON pg_namespace.oid = pg_type.typnamespace WHERE pg_type.oid = pg_depend.refobjid)
                 WHEN 'pg_constraint'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(conname) FROM pg_constraint JOIN pg_namespace ON pg_namespace.oid = pg_constraint.connamespace WHERE pg_constraint.oid = pg_depend.refobjid)
		 WHEN 'pg_namespace'::regclass
		 THEN (SELECT quote_ident(nspname) || '.' FROM pg_namespace WHERE pg_namespace.oid = pg_depend.refobjid)
		 WHEN 'pg_proc'::regclass
		 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(proname) || '(' || pg_get_function_arguments(pg_proc.oid) || ')' FROM pg_proc JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace WHERE pg_proc.oid = pg_depend.refobjid)
		 WHEN 'pg_rewrite'::regclass
		 THEN (SELECT rulename || ' on ' || ev_class::regclass FROM pg_rewrite WHERE pg_rewrite.oid = pg_depend.objid)
                 ELSE refclassid::regclass::text || '#' || refobjid
                 END
            ELSE CASE refclassid
	         WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) || '.' || quote_ident(attname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace JOIN pg_attribute ON pg_attribute.attrelid = pg_class.oid WHERE pg_class.oid = pg_depend.refobjid AND pg_attribute.attnum = pg_depend.refobjsubid)
                 ELSE refclassid::regclass::text || '#' || refobjid || '#' || refobjsubid
                 END
       END,
       CASE WHEN objsubid > 0
            THEN CASE classid
                 WHEN 'pg_class'::regclass
		 THEN (SELECT attname FROM pg_attribute WHERE pg_attribute.attrelid = objid AND pg_attribute.attnum = objsubid)
                 END
            END AS parent_subobject,
       CASE WHEN refobjsubid > 0
            THEN CASE refclassid
                 WHEN 'pg_class'::regclass
		 THEN (SELECT attname FROM pg_attribute WHERE pg_attribute.attrelid = refobjid AND pg_attribute.attnum = refobjsubid)
                 END
            END AS child_subobject,
       EXISTS (SELECT 1 FROM pg_depend more WHERE more.classid = pg_depend.refclassid AND more.objid = pg_depend.refobjid)
FROM pg_depend
WHERE pg_depend.classid = $1 AND pg_depend.objid = $2
UNION ALL
SELECT deptype, refclassid, refobjid, 0, 0,
       CASE refclassid
       WHEN 'pg_authid'::regclass
       THEN (SELECT rolname FROM pg_roles WHERE pg_roles.oid = refobjid)
       ELSE refclassid::regclass::text || '#' || refobjid
       END,
       NULL, NULL,
       EXISTS (SELECT 1 FROM pg_shdepend more WHERE more.classid = pg_shdepend.refclassid AND more.objid = pg_shdepend.refobjid AND more.dbid = $3)
FROM pg_shdepend
WHERE pg_shdepend.classid = $1 AND pg_shdepend.objid = $2 AND pg_shdepend.dbid = $3

-- SQL :: Dependents
SELECT deptype, refclassid, refobjid, refobjsubid, refobjsubid,
       CASE WHEN refobjsubid = 0
            THEN CASE refclassid
                 WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace WHERE pg_class.oid = pg_depend.refobjid)
                 WHEN 'pg_type'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(typname) FROM pg_type JOIN pg_namespace ON pg_namespace.oid = pg_type.typnamespace WHERE pg_type.oid = pg_depend.refobjid)
                 WHEN 'pg_constraint'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(conname) FROM pg_constraint JOIN pg_namespace ON pg_namespace.oid = pg_constraint.connamespace WHERE pg_constraint.oid = pg_depend.refobjid)
		 WHEN 'pg_namespace'::regclass
		 THEN (SELECT quote_ident(nspname) || '.' FROM pg_namespace WHERE pg_namespace.oid = pg_depend.refobjid)
		 WHEN 'pg_proc'::regclass
		 THEN pg_depend.refobjid::regprocedure::text
		 WHEN 'pg_rewrite'::regclass
		 THEN (SELECT rulename || ' on ' || ev_class::regclass FROM pg_rewrite WHERE pg_rewrite.oid = pg_depend.refobjid)
                 ELSE refclassid::regclass::text || '#' || refobjid
                 END
            ELSE CASE refclassid
	         WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) || '.' || quote_ident(attname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace JOIN pg_attribute ON pg_attribute.attrelid = pg_class.oid WHERE pg_class.oid = pg_depend.refobjid AND pg_attribute.attnum = pg_depend.refobjsubid)
                 ELSE refclassid::regclass::text || '#' || refobjid || '#' || refobjsubid
                 END
       END,
       CASE WHEN objsubid > 0
            THEN CASE classid
                 WHEN 'pg_class'::regclass
		 THEN (SELECT attname FROM pg_attribute WHERE pg_attribute.attrelid = objid AND pg_attribute.attnum = objsubid)
                 END
            END AS parent_subobject,
       CASE WHEN refobjsubid > 0
            THEN CASE refclassid
                 WHEN 'pg_class'::regclass
		 THEN (SELECT attname FROM pg_attribute WHERE pg_attribute.attrelid = refobjid AND pg_attribute.attnum = refobjsubid)
                 END
            END AS child_subobject,
       EXISTS (SELECT 1 FROM pg_depend more WHERE more.classid = pg_depend.refclassid AND more.objid = pg_depend.refobjid)
FROM pg_depend
WHERE pg_depend.classid = $1 AND pg_depend.objid = $2
UNION ALL
SELECT deptype, refclassid, refobjid, 0, 0,
       CASE refclassid
       WHEN 'pg_authid'::regclass
       THEN (SELECT rolname FROM pg_roles WHERE pg_roles.oid = refobjid)
       ELSE refclassid::regclass::text || '#' || refobjid
       END,
       NULL, NULL,
       EXISTS (SELECT 1 FROM pg_shdepend more WHERE more.classid = pg_shdepend.refclassid AND more.objid = pg_shdepend.refobjid AND more.dbid = $3)
FROM pg_shdepend
WHERE pg_shdepend.classid = $1 AND pg_shdepend.objid = $2 AND pg_shdepend.dbid = $3

-- SQL :: Dependencies :: 8.4

SELECT deptype, classid, objid, objsubid, refobjsubid,
       CASE WHEN objsubid = 0
            THEN CASE classid
                 WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace WHERE pg_class.oid = pg_depend.objid)
                 WHEN 'pg_type'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(typname) FROM pg_type JOIN pg_namespace ON pg_namespace.oid = pg_type.typnamespace WHERE pg_type.oid = pg_depend.objid)
                 WHEN 'pg_constraint'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(conname) FROM pg_constraint JOIN pg_namespace ON pg_namespace.oid = pg_constraint.connamespace WHERE pg_constraint.oid = pg_depend.objid)
		 WHEN 'pg_namespace'::regclass
		 THEN (SELECT quote_ident(nspname) || '.' FROM pg_namespace WHERE pg_namespace.oid = pg_depend.objid)
		 WHEN 'pg_proc'::regclass
		 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(proname) || '(' || pg_get_function_arguments(pg_proc.oid) || ')' FROM pg_proc JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace WHERE pg_proc.oid = pg_depend.objid)
		 WHEN 'pg_rewrite'::regclass
		 THEN (SELECT rulename || ' on ' || ev_class::regclass FROM pg_rewrite WHERE pg_rewrite.oid = pg_depend.objid)
                 ELSE classid::regclass::text || '#' || objid
                 END
            ELSE CASE classid
	         WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) || '.' || quote_ident(attname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace JOIN pg_attribute ON pg_attribute.attrelid = pg_class.oid WHERE pg_class.oid = pg_depend.objid AND pg_attribute.attnum = pg_depend.objsubid)
                 ELSE classid::regclass::text || '#' || objid || '#' || objsubid
                 END
       END,
       CASE WHEN refobjsubid > 0
            THEN CASE refclassid
                 WHEN 'pg_class'::regclass
		 THEN (SELECT attname FROM pg_attribute WHERE pg_attribute.attrelid = refobjid AND pg_attribute.attnum = refobjsubid)
                 END
            END AS parent_subobject,
       CASE WHEN objsubid > 0
            THEN CASE classid
                 WHEN 'pg_class'::regclass
		 THEN (SELECT attname FROM pg_attribute WHERE pg_attribute.attrelid = objid AND pg_attribute.attnum = objsubid)
                 END
            END AS child_subobject,
       EXISTS (SELECT 1 FROM pg_depend more WHERE more.refclassid = pg_depend.classid AND more.refobjid = pg_depend.objid)
FROM pg_depend
WHERE pg_depend.refclassid = $1 AND pg_depend.refobjid = $2
UNION ALL
SELECT deptype, refclassid, refobjid, 0, 0,
       CASE refclassid
       WHEN 'pg_authid'::regclass
       THEN (SELECT rolname FROM pg_roles WHERE pg_roles.oid = refobjid)
       ELSE refclassid::regclass::text || '#' || refobjid
       END,
       NULL, NULL,
       EXISTS (SELECT 1 FROM pg_shdepend more WHERE more.classid = pg_shdepend.refclassid AND more.objid = pg_shdepend.refobjid AND more.dbid = $3)
FROM pg_shdepend
WHERE pg_shdepend.classid = $1 AND pg_shdepend.objid = $2 AND pg_shdepend.dbid = $3

-- SQL :: Dependencies
SELECT deptype, classid, objid, objsubid, refobjsubid,
       CASE WHEN objsubid = 0
            THEN CASE classid
                 WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace WHERE pg_class.oid = pg_depend.objid)
                 WHEN 'pg_type'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(typname) FROM pg_type JOIN pg_namespace ON pg_namespace.oid = pg_type.typnamespace WHERE pg_type.oid = pg_depend.objid)
                 WHEN 'pg_constraint'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(conname) FROM pg_constraint JOIN pg_namespace ON pg_namespace.oid = pg_constraint.connamespace WHERE pg_constraint.oid = pg_depend.objid)
		 WHEN 'pg_namespace'::regclass
		 THEN (SELECT quote_ident(nspname) || '.' FROM pg_namespace WHERE pg_namespace.oid = pg_depend.objid)
		 WHEN 'pg_proc'::regclass
		 THEN pg_depend.objid::regprocedure::text
		 WHEN 'pg_rewrite'::regclass
		 THEN (SELECT rulename || ' on ' || ev_class::regclass FROM pg_rewrite WHERE pg_rewrite.oid = pg_depend.objid)
                 ELSE classid::regclass::text || '#' || objid
                 END
            ELSE CASE classid
	         WHEN 'pg_class'::regclass
                 THEN (SELECT quote_ident(nspname) || '.' || quote_ident(relname) || '.' || quote_ident(attname) FROM pg_class JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace JOIN pg_attribute ON pg_attribute.attrelid = pg_class.oid WHERE pg_class.oid = pg_depend.objid AND pg_attribute.attnum = pg_depend.objsubid)
                 ELSE classid::regclass::text || '#' || objid || '#' || objsubid
                 END
       END,
       CASE WHEN refobjsubid > 0
            THEN CASE refclassid
                 WHEN 'pg_class'::regclass
		 THEN (SELECT attname FROM pg_attribute WHERE pg_attribute.attrelid = refobjid AND pg_attribute.attnum = refobjsubid)
                 END
            END AS parent_subobject,
       CASE WHEN objsubid > 0
            THEN CASE classid
                 WHEN 'pg_class'::regclass
		 THEN (SELECT attname FROM pg_attribute WHERE pg_attribute.attrelid = objid AND pg_attribute.attnum = objsubid)
                 END
            END AS child_subobject,
       EXISTS (SELECT 1 FROM pg_depend more WHERE more.refclassid = pg_depend.classid AND more.refobjid = pg_depend.objid)
FROM pg_depend
WHERE pg_depend.refclassid = $1 AND pg_depend.refobjid = $2
UNION ALL
SELECT deptype, classid, objid, 0, 0,
       CASE classid
       WHEN 'pg_authid'::regclass
       THEN (SELECT rolname FROM pg_roles WHERE pg_roles.oid = objid)
       ELSE refclassid::regclass::text || '#' || objid
       END,
       NULL, NULL,
       EXISTS (SELECT 1 FROM pg_shdepend more WHERE more.classid = pg_shdepend.classid AND more.objid = pg_shdepend.objid AND more.dbid = $3)
FROM pg_shdepend
WHERE pg_shdepend.refclassid = $1 AND pg_shdepend.refobjid = $2 AND pg_shdepend.dbid = $3

