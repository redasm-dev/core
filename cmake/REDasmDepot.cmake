include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

set(REDASM_DEPOT_DIR "$ENV{REDASM_DEPOT_DIR}" CACHE PATH
    "Directory with pre-extracted dependency sources (one subdir per package)")

option(REDASM_OFFLINE "Never access the network" OFF)
option(REDASM_USE_BUNDLED "Ignore system packages" OFF)

# Same arguments as CPMAddPackage.
# NAME must match the find_package() name.
function(redasm_add_dependency NAME)
    cmake_parse_arguments(PARSE_ARGV 0 RD "" "NAME" "")
    set(NAME "${RD_NAME}")

    if(NOT REDASM_USE_BUNDLED)
        set(CPM_USE_LOCAL_PACKAGES ON)
    endif()

    if(REDASM_OFFLINE)
        set(FETCHCONTENT_FULLY_DISCONNECTED ON)
    endif()

    if(REDASM_DEPOT_DIR AND IS_DIRECTORY "${REDASM_DEPOT_DIR}/${NAME}")
        set(CPM_${NAME}_SOURCE "${REDASM_DEPOT_DIR}/${NAME}")
    elseif(REDASM_OFFLINE)
        if(REDASM_USE_BUNDLED)
            message(FATAL_ERROR "${NAME}: REDASM_OFFLINE with REDASM_USE_BUNDLED needs "
                "'${REDASM_DEPOT_DIR}/${NAME}' (REDASM_DEPOT_DIR is empty if unset)")
        endif()
        set(CPM_LOCAL_PACKAGES_ONLY ON)   # system package or error, never fetch
    endif()

    CPMAddPackage(${ARGV})

    set(${NAME}_ADDED      "${${NAME}_ADDED}"      PARENT_SCOPE)
    set(${NAME}_SOURCE_DIR "${${NAME}_SOURCE_DIR}" PARENT_SCOPE)
    set(${NAME}_BINARY_DIR "${${NAME}_BINARY_DIR}" PARENT_SCOPE)
endfunction()

