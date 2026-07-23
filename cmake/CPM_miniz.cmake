find_package(miniz QUIET)

if(NOT miniz_FOUND)
    CPMAddPackage(
        NAME miniz
        GITHUB_REPOSITORY richgel999/miniz
        GIT_TAG 3.1.1

        OPTIONS
            "CMAKE_POSITION_INDEPENDENT_CODE ON"
    )

    if(NOT TARGET miniz::miniz)
        add_library(miniz::miniz ALIAS miniz)
    endif()
else()
    message(STATUS "Using system miniz")
endif()
