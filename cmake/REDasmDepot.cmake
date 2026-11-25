include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

set(REDASM_DEPOT_DIR "$ENV{REDASM_DEPOT_DIR}" CACHE PATH
    "Directory with pre-extracted dependency sources (one subdir per package)")

option(REDASM_OFFLINE "Never access the network" OFF)
option(REDASM_USE_BUNDLED "Ignore system packages" OFF)

# Same arguments as CPMAddPackage.
# NAME must match the find_package() name.
function(redasm_add_dependency NAME)
    cmake_parse_arguments(PARSE_ARGV 0 RD "" "NAME;VERSION;DOWNLOAD_ONLY" "")
    set(NAME "${RD_NAME}")
    set(_cpm_args ${ARGV})

    if(NOT REDASM_USE_BUNDLED)
        set(CPM_USE_LOCAL_PACKAGES ON)
    endif()

    if(REDASM_OFFLINE)
        set(FETCHCONTENT_FULLY_DISCONNECTED ON)
    endif()

    if(REDASM_DEPOT_DIR AND IS_DIRECTORY "${REDASM_DEPOT_DIR}/${NAME}")
        string(TOUPPER "${NAME}" _upper)
        set(FETCHCONTENT_SOURCE_DIR_${_upper} "${REDASM_DEPOT_DIR}/${NAME}")

        # DOWNLOAD_ONLY packages are populated by CPM (CMake >= 3.30.3) with
        # FetchContent_Populate(<name> <details>), which ignores both
        # FETCHCONTENT_SOURCE_DIR_<NAME> and FETCHCONTENT_FULLY_DISCONNECTED:
        # point them straight at the depot, without any download information
        if(RD_DOWNLOAD_ONLY)
            set(_cpm_args NAME "${NAME}" SOURCE_DIR "${REDASM_DEPOT_DIR}/${NAME}" DOWNLOAD_ONLY YES)
            if(RD_VERSION)
                list(APPEND _cpm_args VERSION "${RD_VERSION}")
            endif()
        endif()
    elseif(REDASM_OFFLINE)
        if(REDASM_USE_BUNDLED)
            message(FATAL_ERROR "${NAME}: REDASM_OFFLINE with REDASM_USE_BUNDLED needs "
                "'${REDASM_DEPOT_DIR}/${NAME}' (REDASM_DEPOT_DIR is empty if unset)")
        endif()
        set(CPM_LOCAL_PACKAGES_ONLY ON)   # system package or error, never fetch
    endif()

    CPMAddPackage(${_cpm_args})

    set(${NAME}_ADDED      "${${NAME}_ADDED}"      PARENT_SCOPE)
    set(${NAME}_SOURCE_DIR "${${NAME}_SOURCE_DIR}" PARENT_SCOPE)
    set(${NAME}_BINARY_DIR "${${NAME}_BINARY_DIR}" PARENT_SCOPE)
endfunction()

