string(TIMESTAMP REDASM_BUILD_TIMESTAMP "%Y%m%d")

set(REDASM_GIT_VERSION "")
set(REDASM_VERSION_MAJOR 4)
set(REDASM_VERSION_MINOR 0)
set(REDASM_VERSION_PATCH 0)
set(REDASM_VERSION_SUFFIX "DEV")

find_package(Git QUIET)

# CI release pipeline override: the workspace release workflow already knows
# the exact tag (e.g. "v4.0.1" or "v4.0.1-beta1") being built.
# Used in release.yml
#   cmake -B build -DREDASM_RELEASE_TAG=v4.0.1
#   cmake -B build -DREDASM_RELEASE_TAG=v4.0.0-beta1

if(DEFINED REDASM_RELEASE_TAG AND REDASM_RELEASE_TAG MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)(-(.+))?$")
    set(REDASM_VERSION_MAJOR "${CMAKE_MATCH_1}")
    set(REDASM_VERSION_MINOR "${CMAKE_MATCH_2}")
    set(REDASM_VERSION_PATCH "${CMAKE_MATCH_3}")
    set(REDASM_VERSION_SUFFIX "${CMAKE_MATCH_5}")
elseif(GIT_FOUND) # nightly or local build
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE REDASM_GIT_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(REDASM_GIT_VERSION "nogit")
endif()

set(REDASM_VERSION "${REDASM_VERSION_MAJOR}.${REDASM_VERSION_MINOR}.${REDASM_VERSION_PATCH}")

if(NOT REDASM_VERSION_SUFFIX STREQUAL "")
    set(REDASM_VERSION "${REDASM_VERSION}-${REDASM_VERSION_SUFFIX}")
endif()

if(REDASM_GIT_VERSION STREQUAL "")
    set(REDASM_BUILD "${REDASM_VERSION} (${REDASM_BUILD_TIMESTAMP})")
else()
    set(REDASM_BUILD "${REDASM_VERSION} (${REDASM_BUILD_TIMESTAMP}.${REDASM_GIT_VERSION})")
endif()

function(redasm_setup_version target)
    target_compile_definitions(${target} PRIVATE
        RD_VERSION="${REDASM_VERSION}"
        RD_BUILD_VERSION="${REDASM_BUILD}"
    )
endfunction()
