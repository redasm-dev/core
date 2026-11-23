function(redasm_stage target dest)
    add_custom_target(stage_${target} ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dest}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target}>" "${dest}/"
        COMMENT "Staging ${target} -> ${dest}"
        DEPENDS ${target}
    )
endfunction()

function(redasm_install_plugins category)
    if(WIN32)
        set(_dest "plugins/${category}")
    else()
        include(GNUInstallDirs)
        set(_dest "${CMAKE_INSTALL_LIBDIR}/redasm/plugins/${category}")
    endif()

    if(APPLE)
        set(_rpath_base "@loader_path")
    else()
        set(_rpath_base "$ORIGIN")
    endif()

    get_property(_subdirs DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY SUBDIRECTORIES)

    foreach(_subdir ${_subdirs})
        get_property(_targets DIRECTORY ${_subdir} PROPERTY BUILDSYSTEM_TARGETS)

        foreach(_target ${_targets})
            get_target_property(_type ${_target} TYPE)

            if(_type STREQUAL "SHARED_LIBRARY")
                if(NOT WIN32)
                    set_target_properties(${_target} PROPERTIES
                                          INSTALL_RPATH "${_rpath_base}/../../../")
                endif()

                install(TARGETS ${_target}
                        LIBRARY DESTINATION "${_dest}"
                        RUNTIME DESTINATION "${_dest}")
            endif()
        endforeach()
    endforeach()
endfunction()

function(redasm_install_executable target)
    include(GNUInstallDirs)

    if(NOT WIN32)
        if(APPLE)
            set(_base "@loader_path")
        else()
            set(_base "$ORIGIN")
        endif()

        file(RELATIVE_PATH _rel_libdir
             "${CMAKE_INSTALL_FULL_BINDIR}"
             "${CMAKE_INSTALL_FULL_LIBDIR}")

        set(_rpath "${_base}")
        if(_rel_libdir AND NOT _rel_libdir STREQUAL ".")
            list(APPEND _rpath "${_base}/${_rel_libdir}")
        endif()

        if(APPLE)
            list(APPEND _rpath "${_base}/../Frameworks")
        endif()

        list(REMOVE_DUPLICATES _rpath)

        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "${_rpath}"
            INSTALL_RPATH "${_rpath}"
            BUILD_WITH_INSTALL_RPATH FALSE
        )
    endif()

    install(TARGETS ${target}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # set by an aggregate build (the workspace), unset in standalone builds
    if(REDASM_STAGE_DIR)
        redasm_stage(${target} "${REDASM_STAGE_DIR}")
    endif()
endfunction()
