# file group for core/database/sql/
filegroup(
    name = "database_schema_sql",
    srcs = glob(["*.sql"]),
    visibility = ["//visibility:public"],
)

exports_files([
    "V001__init.sql",
    "V002__rls.sql",
    "V003__realtime.sql",
])
