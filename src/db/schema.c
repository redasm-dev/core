#include "schema.h"

const char DB_SCHEMA[] = " \
PRAGMA page_size = 8192; \
PRAGMA synchronous = OFF; \
PRAGMA journal_mode = MEMORY; \
PRAGMA cache_size = -65536; \
PRAGMA foreign_keys = ON; \
PRAGMA temp_store = MEMORY; \
\
CREATE TABLE IF NOT EXISTS Segments ( \
    name          TEXT NOT NULL, \
    start_address INTEGER NOT NULL, \
    end_address   INTEGER NOT NULL, \
    perm          INTEGER NOT NULL \
); \
CREATE TABLE IF NOT EXISTS InputMappings ( \
    offset        INTEGER NOT NULL, \
    start_address INTEGER NOT NULL, \
    end_address   INTEGER NOT NULL \
); \
\
CREATE TABLE IF NOT EXISTS Comments ( \
    address   INTEGER NOT NULL, \
    placement INTEGER NOT NULL, \
    line      INTEGER NOT NULL, \
    comment   TEXT NOT NULL, \
    PRIMARY KEY(address, placement, line) \
); \
\
CREATE TABLE IF NOT EXISTS XRefs ( \
    from_address INTEGER NOT NULL, \
    to_address   INTEGER NOT NULL, \
    type         INTEGER NOT NULL, \
    confidence   INTEGER NOT NULL, \
    UNIQUE(from_address, to_address) \
); \
\
CREATE TABLE IF NOT EXISTS TypeDefs( \
    id        INTEGER PRIMARY KEY AUTOINCREMENT, \
    name      TEXT UNIQUE NOT NULL, \
    kind      INTEGER NOT NULL, \
    is_noret  INTEGER NOT NULL, \
    enum_type TEXT \
); \
\
CREATE TABLE IF NOT EXISTS TypeParams ( \
    owner      TEXT NOT NULL REFERENCES TypeDefs(name), \
    type       TEXT NOT NULL, \
    name       TEXT NOT NULL, \
    count      INTEGER NOT NULL, \
    modifier   INTEGER NOT NULL, \
    member_idx INTEGER NOT NULL, \
    PRIMARY KEY(owner, member_idx) \
); \
\
CREATE TABLE IF NOT EXISTS TypeEnum ( \
    owner TEXT NOT NULL REFERENCES TypeDefs(name), \
    name  TEXT NOT NULL, \
    value INTEGER NOT NULL, \
    PRIMARY KEY(owner, name) \
); \
\
CREATE TABLE IF NOT EXISTS Types ( \
    address    INTEGER PRIMARY KEY, \
    name       TEXT NOT NULL, \
    count      INTEGER NOT NULL, \
    modifier   INTEGER NOT NULL, \
    confidence INTEGER NOT NULL \
); \
\
CREATE TABLE IF NOT EXISTS Names ( \
    address    INTEGER PRIMARY KEY, \
    name       TEXT NOT NULL, \
    confidence INTEGER NOT NULL \
); \
CREATE TABLE IF NOT EXISTS Externals ( \
    address INTEGER PRIMARY KEY, \
    ordinal INTEGER, \
    module  TEXT, \
    kind    INTEGER NOT NULL \
); \
CREATE TABLE IF NOT EXISTS Functions ( \
    address   INTEGER PRIMARY KEY, \
    type_name TEXT \
); \
\
CREATE TABLE IF NOT EXISTS SegmentRegisters ( \
    address  INTEGER NOT NULL, \
    reg      TEXT NOT NULL, \
    value    INTEGER, \
    confidence INTEGER NOT NULL, \
    PRIMARY KEY(address, reg) \
); \
CREATE TABLE IF NOT EXISTS OperandOverrides ( \
    address  INTEGER NOT NULL, \
    idx      INTEGER NOT NULL, \
    PRIMARY KEY (address, idx) \
); \
\
CREATE TABLE IF NOT EXISTS Problems ( \
    from_address INTEGER NOT NULL, \
    address      INTEGER NOT NULL, \
    message      TEXT NOT NULL \
); \
CREATE INDEX IF NOT EXISTS Names_NameIdx ON Names(name); \
CREATE INDEX IF NOT EXISTS XRefs_ToIdx ON XRefs(to_address, from_address, type); \
";
