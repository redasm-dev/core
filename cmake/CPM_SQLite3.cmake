CPMAddPackage(
    NAME sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3470100.zip
    URL_HASH SHA3_256=71c08f4c890000094a6781169927de8f87ad8569410d9a4310c07dbca1f14b37
)

if(sqlite3_ADDED)
    add_library(sqlite3 STATIC "${sqlite3_SOURCE_DIR}/sqlite3.c")
    target_include_directories(sqlite3 PUBLIC "${sqlite3_SOURCE_DIR}")
    set_target_properties(sqlite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
