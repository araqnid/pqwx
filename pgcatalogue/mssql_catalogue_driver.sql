SELECT object_id, N't' AS objtype, schemas.name + N'.' + tables.name AS objname
FROM sys.tables
     JOIN sys.schemas ON schemas.schema_id = tables.schema_id

SELECT object_id, N'v' AS objtype, schemas.name + N'.' + views.name AS objname
FROM sys.views
     JOIN sys.schemas ON schemas.schema_id = views.schema_id

SELECT object_id, N'fp' AS objtype, schemas.name + N'.' + procedures.name AS objname
FROM sys.procedures
     JOIN sys.schemas ON schemas.schema_id = procedures.schema_id

\go
