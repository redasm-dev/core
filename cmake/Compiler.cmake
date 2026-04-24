function(setup_compiler project_name)
    set_target_properties(${project_name}
        PROPERTIES
            EXPORT_COMPILE_COMMANDS ON
            C_STANDARD 17
    )

    set(NOT_MSVC_COMPILE_OPTIONS
        "-Wall"
        "-Wextra"
        "-Wpedantic"
        "-Wno-missing-field-initializers"
        "-Wno-error=unused"
        "-Wno-error=unused-function"
        "-Wno-error=unused-parameter"
        "-Wno-error=unused-value"
        "-Wno-error=unused-variable"
        "-Wno-error=unused-local-typedefs"
        "-Wno-error=unused-but-set-parameter"
        "-Wno-error=unused-but-set-variable"
        "-Wno-error=unused-const-variable"
    )

    set(NOT_MSVC_COMPILE_OPTIONS_DEBUG
        "-Werror"
        "-g"
        "-fsanitize=address,undefined"
        "-fno-omit-frame-pointer"
    )

    set(NOT_MSVC_LINK_OPTIONS_DEBUG
        "-fsanitize=address,undefined"
        "-fno-omit-frame-pointer"
    )

    set(MSVC_COMPILE_OPTIONS "/W4")

    set(MSVC_COMPILE_OPTIONS_DEBUG
        "/WX"
        "/Od"
        "/fsanitize=address"
    )

    target_compile_options(${project_name}
        PRIVATE
            $<$<C_COMPILER_ID:MSVC>:${MSVC_COMPILE_OPTIONS}>
            $<$<NOT:$<C_COMPILER_ID:MSVC>>:${NOT_MSVC_COMPILE_OPTIONS}>
            $<$<AND:$<C_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:${MSVC_COMPILE_OPTIONS_DEBUG}>
            $<$<AND:$<NOT:$<C_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:${NOT_MSVC_COMPILE_OPTIONS_DEBUG}>
    )

    target_link_options(${project_name}
        PUBLIC
            $<$<AND:$<NOT:$<C_COMPILER_ID:MSVC>>,$<CONFIG:Debug>>:${NOT_MSVC_LINK_OPTIONS_DEBUG}>
    )
endfunction()
