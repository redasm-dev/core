#include "schema.h"

const char DB_SCHEMA[] = " \
PRAGMA synchronous = OFF; \
PRAGMA journal_mode = MEMORY; \
PRAGMA foreign_keys = ON; \
\
CREATE TABLE Info ( \
    key TEXT PRIMARY KEY, \
    str_val TEXT, \
    int_val INTEGER, \
    CHECK((str_val IS NULL) != (int_val IS NULL)) \
); \
\
CREATE TABLE UserData ( \
    k TEXT PRIMARY KEY,  \
    v INTEGER NOT NULL \
); \
\
CREATE TABLE Segments ( \
    name          TEXT NOT NULL, \
    start_address INTEGER NOT NULL, \
    end_address   INTEGER NOT NULL, \
    unit          INTEGER NOT NULL, \
    perm          INTEGER NOT NULL \
); \
CREATE TABLE InputMappings ( \
    offset        INTEGER NOT NULL, \
    start_address INTEGER NOT NULL, \
    end_address   INTEGER NOT NULL \
); \
\
CREATE TABLE Comments ( \
    address INTEGER PRIMARY KEY, \
    comment TEXT NOT NULL \
); \
\
CREATE TABLE XRefs ( \
    from_address INTEGER NOT NULL, \
    to_address   INTEGER NOT NULL, \
    type         INTEGER NOT NULL, \
    UNIQUE(from_address, to_address) \
); \
\
CREATE TABLE TypeDefs( \
    name      TEXT PRIMARY KEY, \
    kind      INTEGER NOT NULL, \
    size      INTEGER NOT NULL, \
    is_noret  INTEGER NOT NULL, \
    enum_type TEXT \
); \
\
CREATE TABLE TypeParams ( \
    owner      TEXT NOT NULL REFERENCES TypeDefs(name), \
    type       TEXT NOT NULL, \
    name       TEXT NOT NULL, \
    count      INTEGER NOT NULL, \
    flags      INTEGER NOT NULL, \
    member_idx INTEGER NOT NULL, \
    PRIMARY KEY(owner, member_idx) \
); \
\
CREATE TABLE TypeEnum ( \
    owner TEXT NOT NULL REFERENCES TypeDefs(name), \
    name  TEXT NOT NULL, \
    value INTEGER NOT NULL, \
    PRIMARY KEY(owner, name) \
); \
\
CREATE TABLE Types ( \
    address    INTEGER PRIMARY KEY, \
    name       TEXT NOT NULL, \
    count      INTEGER NOT NULL, \
    flags      INTEGER NOT NULL, \
    confidence INTEGER NOT NULL \
); \
\
CREATE TABLE Names ( \
    address    INTEGER PRIMARY KEY, \
    name       TEXT NOT NULL, \
    confidence INTEGER NOT NULL \
); \
CREATE TABLE Imported ( \
    address INTEGER PRIMARY KEY, \
    ordinal INTEGER, \
    module  TEXT \
); \
\
CREATE TABLE SegmentRegisters ( \
    address  INTEGER NOT NULL, \
    reg      INTEGER NOT NULL, \
    value    INTEGER, \
    fromaddr INTEGER, \
    UNIQUE(address, reg, fromaddr) \
); \
\
CREATE TABLE Problems ( \
    from_address INTEGER NOT NULL, \
    address  INTEGER NOT NULL, \
    message  TEXT NOT NULL \
); \
";
