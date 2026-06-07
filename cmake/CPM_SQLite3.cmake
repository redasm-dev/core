find_package(SQLite3 QUIET)

if(NOT SQLite3_FOUND)
    CPMAddPackage(
        NAME SQLite3
        URL https://sqlite.org/2026/sqlite-amalgamation-3530100.zip
        URL_HASH SHA3_256=3c07136e4f6b5dd0c395be86455014039597bc65b6851f7111e88f71b6e06114
    )

    if(SQLite3_ADDED)
        add_library(SQLite3 STATIC "${SQLite3_SOURCE_DIR}/sqlite3.c")
        target_include_directories(SQLite3 PUBLIC "${SQLite3_SOURCE_DIR}")
        set_target_properties(SQLite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
        add_library(SQLite3::SQLite3 ALIAS SQLite3)
    endif()
else()
    # https://cmake.org/cmake/help/latest/module/FindSQLite3.html
    if(NOT TARGET SQLite3::SQLite3)
        add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
    endif()
    message(STATUS "Using system SQLite3")
endif()
