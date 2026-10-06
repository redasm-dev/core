redasm_add_dependency(
    NAME SQLite3
    VERSION 3.35
    URL https://sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e
)

if(SQLite3_ADDED)
    add_library(SQLite3 STATIC "${SQLite3_SOURCE_DIR}/sqlite3.c")
    target_include_directories(SQLite3 PUBLIC "${SQLite3_SOURCE_DIR}")
    set_target_properties(SQLite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(SQLite3::SQLite3 ALIAS SQLite3)
elseif(NOT TARGET SQLite3::SQLite3)
    # https://cmake.org/cmake/help/latest/module/FindSQLite3.html
    add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
endif()
