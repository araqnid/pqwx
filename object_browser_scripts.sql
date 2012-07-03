-- SQL used for script generation by the object browser

-- SQL :: Database Detail
SELECT datname,
       owner.rolname,
       pg_encoding_to_char(encoding),
       datcollate,
       datctype,
       datconnlimit,
       datacl,
       pg_catalog.shobj_description(pg_database.oid, 'pg_database')
FROM pg_database
     JOIN pg_roles owner ON owner.oid = pg_database.datdba
WHERE pg_database.oid = $1

-- SQL :: Database Settings :: 9.1
SELECT pg_roles.rolname, setconfig
FROM pg_db_role_setting LEFT JOIN pg_roles ON pg_roles.oid = pg_db_role_setting.setrole
WHERE pg_db_role_setting.setdatabase = $1

-- SQL :: Database Settings
SELECT '', datconfig
FROM pg_database
WHERE pg_database.oid = $1

-- SQL :: Table Detail :: 9.1
SELECT relname,
       nspname,
       owner.rolname,
       pg_tablespace.spcname,
       relhasoids,
       relacl,
       reloptions,
       pg_catalog.obj_description(pg_class.oid, 'pg_class'),
       relpersistence = 'u' AS is_unlogged
FROM pg_class
     JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace
     JOIN pg_roles owner ON owner.oid = pg_class.relowner
     LEFT JOIN pg_tablespace ON pg_tablespace.oid = pg_class.reltablespace
WHERE pg_class.oid = $1

-- SQL :: Table Detail
SELECT relname,
       nspname,
       owner.rolname,
       pg_tablespace.spcname,
       relhasoids,
       relacl,
       reloptions,
       pg_catalog.obj_description(pg_class.oid, 'pg_class'),
       false AS is_unlogged
FROM pg_class
     JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace
     JOIN pg_roles owner ON owner.oid = pg_class.relowner
     LEFT JOIN pg_tablespace ON pg_tablespace.oid = pg_class.reltablespace
WHERE pg_class.oid = $1

-- SQL :: Schema Detail
SELECT nspname,
       owner.rolname,
       nspacl
FROM pg_namespace
     JOIN pg_roles owner ON owner.oid = pg_namespace.nspowner
WHERE pg_namespace.oid = $1

-- SQL :: Relation Column Detail :: 9.1
SELECT attname,
       pg_catalog.format_type(atttypid, atttypmod),
       attnotnull,
       atthasdef,
       pg_get_expr(pg_attrdef.adbin, pg_attrdef.adrelid),
       CASE WHEN attcollation <> typcollation THEN quote_ident(collname) END,
       attstattarget,
       CASE attstorage WHEN 'e' THEN 'EXTERNAL' END,
       attacl,
       attoptions,
       pg_catalog.col_description(attrelid, attnum)
FROM pg_attribute
     LEFT JOIN pg_attrdef ON pg_attrdef.adrelid = pg_attribute.attrelid AND pg_attrdef.adnum = pg_attribute.attnum
     LEFT JOIN pg_collation ON pg_collation.oid = pg_attribute.attcollation
     JOIN pg_type ON pg_type.oid = pg_attribute.atttypid
WHERE pg_attribute.attrelid = $1
      AND NOT attisdropped
      AND attnum > 0
ORDER BY attnum

-- SQL :: Relation Column Detail
SELECT attname,
       pg_catalog.format_type(atttypid, atttypmod),
       attnotnull,
       atthasdef,
       pg_get_expr(pg_attrdef.adbin, pg_attrdef.adrelid),
       NULL AS collname,
       attstattarget,
       CASE attstorage WHEN 'e' THEN 'EXTERNAL' END,
       NULL AS attacl,
       NULL AS attoptions,
       pg_catalog.col_description(attrelid, attnum)
FROM pg_attribute
     LEFT JOIN pg_attrdef ON pg_attrdef.adrelid = pg_attribute.attrelid AND pg_attrdef.adnum = pg_attribute.attnum
     JOIN pg_type ON pg_type.oid = pg_attribute.atttypid
WHERE pg_attribute.attrelid = $1
      AND NOT attisdropped
      AND attnum > 0
ORDER BY attnum

-- SQL :: View Detail
SELECT relname,
       nspname,
       owner.rolname,
       rtrim(pg_get_viewdef(pg_class.oid, true), ';'),
       relacl,
       pg_catalog.obj_description(pg_class.oid, 'pg_class')
FROM pg_class
     JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace
     JOIN pg_roles owner ON owner.oid = pg_class.relowner
WHERE pg_class.oid = $1

-- SQL :: Sequence Name
SELECT quote_ident(nspname) || '.' || quote_ident(relname)
FROM pg_class
     JOIN pg_namespace ON pg_namespace.oid = pg_class.relnamespace
WHERE pg_class.oid = $1

-- SQL :: Sequence Detail
SELECT owner.rolname,
       pg_class.relacl,
       increment_by,
       min_value,
       max_value,
       start_value,
       cache_value,
       is_cycled,
       COALESCE(quote_ident(owning_table_schema.nspname) || quote_ident(owning_table.relname) || '.' || quote_ident(owning_column.attname), ''),
       pg_catalog.obj_description(pg_class.oid, 'pg_class')
FROM $sequence AS seq,
     pg_class
     JOIN pg_roles owner ON owner.oid = pg_class.relowner
     LEFT JOIN pg_depend ON pg_depend.classid = 'pg_class'::regclass
                            AND pg_depend.objid = pg_class.oid
                            AND pg_depend.deptype = 'a'
                            AND pg_depend.refclassid = 'pg_class'::regclass
     LEFT JOIN pg_class owning_table ON owning_table.oid = pg_depend.refobjid
     LEFT JOIN pg_namespace owning_table_schema ON owning_table_schema.oid = owning_table.relnamespace
     LEFT JOIN pg_attribute owning_column ON owning_column.attrelid = pg_depend.refobjid
                                             AND owning_column.attnum = pg_depend.refobjsubid
WHERE pg_class.oid = $1

-- SQL :: Function Detail
SELECT proname,
       nspname,
       pg_get_function_arguments(pg_proc.oid),
       pg_proc.prorettype,
       pg_proc.proargtypes,
       pg_proc.proallargtypes,
       pg_proc.proargmodes,
       pg_proc.proargnames,
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
       proacl,
       pg_catalog.obj_description(pg_proc.oid, 'pg_proc'),
       proisagg,
       aggtransfn::regprocedure,
       aggtranstype::regtype,
       case when aggfinalfn > 0 then aggfinalfn::regprocedure end,
       agginitval,
       case when aggsortop > 0 then aggsortop::regoper end
FROM pg_proc
     JOIN pg_namespace ON pg_namespace.oid = pg_proc.pronamespace
     JOIN pg_roles owner ON owner.oid = pg_proc.proowner
     LEFT JOIN pg_aggregate ON pg_aggregate.aggfnoid = pg_proc.oid
WHERE pg_proc.oid = $1

-- SQL :: Type Info
SELECT nspname, CASE WHEN pg_type.typelem > 0 THEN elemtype.typname ELSE pg_type.typname END AS base_type_name, CASE WHEN pg_type.typelem > 0 THEN 1 ELSE 0 END AS array_depth
FROM pg_type
     JOIN pg_namespace ON pg_namespace.oid = pg_type.typnamespace
     LEFT JOIN pg_type elemtype ON elemtype.oid = pg_type.typelem
WHERE pg_type.oid = $1

-- SQL :: Table Foreign Keys
SELECT conname,
       confupdtype,
       confdeltype,
       confmatchtype,
       src.attname AS srcatt,
       dstrel.relname AS dstrelname,
       dstnsp.nspname AS dstnspname,
       dst.attname AS dstatt
FROM pg_constraint
     CROSS JOIN generate_series(1,64) g
     JOIN pg_attribute src ON src.attrelid = pg_constraint.conrelid AND src.attnum = conkey[g]
     JOIN pg_attribute dst ON dst.attrelid = pg_constraint.confrelid AND dst.attnum = confkey[g]
     JOIN pg_class dstrel ON dstrel.oid = pg_constraint.confrelid
     JOIN pg_namespace dstnsp ON dstnsp.oid = dstrel.relnamespace
WHERE conrelid = $1
      AND contype = 'f'
      AND g BETWEEN array_lower(conkey, 1)
      AND array_upper(conkey, 1)
ORDER BY pg_constraint.oid, g

-- SQL :: Text Search Dictionary Detail
SELECT dictname, dict_namespace.nspname, owner.rolname AS owner,
       tmplname, tmpl_namespace.nspname,
       dictinitoption
FROM pg_ts_dict
     JOIN pg_namespace dict_namespace ON dict_namespace.oid = pg_ts_dict.dictnamespace
     JOIN pg_roles owner ON owner.oid = pg_ts_dict.dictowner
     JOIN pg_ts_template ON pg_ts_template.oid = pg_ts_dict.dicttemplate
     JOIN pg_namespace tmpl_namespace ON tmpl_namespace.oid = pg_ts_template.tmplnamespace
WHERE pg_ts_dict.oid = $1

-- SQL :: Text Search Template Detail
SELECT tmplname, tmpl_namespace.nspname,
       tmplinit, tmpllexize
FROM pg_ts_template
     JOIN pg_namespace tmpl_namespace ON tmpl_namespace.oid = pg_ts_template.tmplnamespace
WHERE pg_ts_template.oid = $1

-- SQL :: Text Search Parser Detail
SELECT prsname, prs_namespace.nspname,
       prsstart, prstoken, prsend, prslextype, prsheadline
FROM pg_ts_parser
     JOIN pg_namespace prs_namespace ON prs_namespace.oid = pg_ts_parser.prsnamespace
WHERE pg_ts_parser.oid = $1

-- SQL :: Text Search Configuration Detail
SELECT cfgname, cfg_namespace.nspname, owner.rolname AS owner,
       prsname, prs_namespace.nspname
FROM pg_ts_config
     JOIN pg_namespace cfg_namespace ON cfg_namespace.oid = pg_ts_config.cfgnamespace
     JOIN pg_roles owner ON owner.oid = pg_ts_config.cfgowner
     JOIN pg_ts_parser ON pg_ts_parser.oid = pg_ts_config.cfgparser
     JOIN pg_namespace prs_namespace ON prs_namespace.oid = pg_ts_parser.prsnamespace
WHERE pg_ts_config.oid = $1

-- SQL :: Text Search Configuration Mappings
SELECT maptokentype, dictname, dict_namespace.nspname
FROM pg_ts_config_map
     JOIN pg_ts_dict ON pg_ts_dict.oid = pg_ts_config_map.mapdict
     JOIN pg_namespace dict_namespace ON dict_namespace.oid = pg_ts_dict.dictnamespace
WHERE pg_ts_config_map.mapcfg = $1

-- SQL :: Tablespace Detail :: 9.2
SELECT spcname, owner.rolname AS owner,
       spcacl, pg_catalog.pg_tablespace_location(pg_tablespace.oid), spcoptions
FROM pg_tablespace
     JOIN pg_roles owner ON owner.oid = pg_tablespace.spcowner
WHERE pg_tablespace.oid = $1

-- SQL :: Tablespace Detail
SELECT spcname, owner.rolname AS owner,
       spcacl, spclocation, spcoptions
FROM pg_tablespace
     JOIN pg_roles owner ON owner.oid = pg_tablespace.spcowner
WHERE pg_tablespace.oid = $1

-- SQL :: Role Detail
SELECT rolname,
       rolsuper, rolinherit, rolcreaterole, rolcreatedb,
       rolcanlogin, rolreplication, rolconnlimit, rolvaliduntil,
       rolconfig
FROM pg_roles
WHERE pg_roles.oid = $1

-- SQL :: Role Memberships
SELECT granted_role.rolname, grantor_role.rolname,
       pg_auth_members.admin_option
FROM pg_auth_members
     JOIN pg_roles granted_role ON granted_role.oid = pg_auth_members.roleid
     JOIN pg_roles grantor_role ON grantor_role.oid = pg_auth_members.grantor
WHERE pg_auth_members.member = $1

-- SQL :: Role PerDatabase Settings
SELECT pg_database.datname, setconfig
FROM pg_db_role_setting
     JOIN pg_database ON pg_database.oid = pg_db_role_setting.setdatabase
WHERE pg_db_role_setting.setrole = $1
