redasm_add_dependency(
    NAME miniz
    GITHUB_REPOSITORY richgel999/miniz
    GIT_TAG 3.1.1

    OPTIONS
        "CMAKE_POSITION_INDEPENDENT_CODE ON"
)

if(miniz_ADDED AND NOT TARGET miniz)
    add_library(miniz STATIC "${miniz_SOURCE_DIR}/miniz.c")
    target_include_directories(miniz PUBLIC "${miniz_SOURCE_DIR}")
    set_target_properties(miniz PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(miniz_ADDED AND NOT TARGET miniz::miniz)
    add_library(miniz::miniz ALIAS miniz)
endif()
