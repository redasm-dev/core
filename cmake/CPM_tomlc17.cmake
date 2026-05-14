CPMAddPackage(
    NAME tomlc17
    GIT_TAG "R20260501"
    GITHUB_REPOSITORY "cktan/tomlc17"
)

if(tomlc17_ADDED)
    add_library(tomlc17 STATIC "${tomlc17_SOURCE_DIR}/src/tomlc17.c")
    target_include_directories(tomlc17 PUBLIC "${tomlc17_SOURCE_DIR}/src")
    set_target_properties(tomlc17 PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
