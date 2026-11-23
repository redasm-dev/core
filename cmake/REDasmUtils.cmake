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

        set(_rpath "${_base}" "${_base}/${_rel_libdir}")

        if(APPLE)
            list(APPEND _rpath "${_base}/../Frameworks")
        endif()

        set_target_properties(${target} PROPERTIES INSTALL_RPATH "${_rpath}")
    endif()

    install(TARGETS ${target}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction()
