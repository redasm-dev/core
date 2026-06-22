include(cmake/CPM.cmake)
include(cmake/CPM_SQLite3.cmake)
include(cmake/CPM_tomlc17.cmake)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

function(setup_dependencies)
    CPMAddPackage(
        NAME miniz
        GITHUB_REPOSITORY richgel999/miniz
        GIT_TAG 3.1.1

        OPTIONS
            "CMAKE_POSITION_INDEPENDENT_CODE ON"
    )
endfunction()
