cmake_minimum_required(VERSION 3.25)

project(redasm C)

include(cmake/Dependencies.cmake)
setup_dependencies()

add_library(${PROJECT_NAME} SHARED)
add_library(redasm::redasm ALIAS ${PROJECT_NAME})

include(cmake/Compiler.cmake)
setup_compiler(${PROJECT_NAME})

include(CheckLibraryExists)
check_library_exists(m log "" HAVE_LIBM)

target_sources(${PROJECT_NAME} 
    PRIVATE
        src/core/db/db.c
        src/core/db/schema.c
        src/core/context.c
        src/core/engine.c
        src/core/function.c
        src/core/mapping.c
        src/core/segment.c
        src/core/state.c
        src/core/stringfinder.c
        src/core/worker.c
        src/graphs/graph.c
        src/graphs/layered.c
        src/io/buffer.c
        src/io/flags.c
        src/io/flagsbuffer.c
        src/io/reader.c
        src/listing/builder.c
        src/listing/listing.c
        src/plugins/builtin/analyzers.c
        src/plugins/builtin/loaders.c
        src/plugins/builtin/processors.c
        src/plugins/command.c
        src/plugins/module.c
        src/rdil/rdil.c
        src/support/byteorder.c
        src/support/containers.c
        src/support/error.c
        src/support/hash.c
        src/support/logging.c
        src/support/pattern.c
        src/support/utils.c
        src/support/stringpool.c
        src/surface/graph.c
        src/surface/items.c
        src/surface/path.c
        src/surface/renderer.c
        src/surface/state.c
        src/surface/surface.c
        src/types/def.c
        src/types/type.c
        src/redasm.c
        src/theme.c
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>

    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        sqlite3
)

if(HAVE_LIBM)
    target_link_libraries(${PROJECT_NAME} PRIVATE m)
endif()

include(GNUInstallDirs)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY include/redasm
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/redasmConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/redasmConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/redasm
)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/redasmConfig.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/redasm
)
