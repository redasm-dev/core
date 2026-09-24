macro(rd_no_network reason)
    set(REDASM_NETWORK_SOURCES 
        src/net/http/backend/stub.c
        src/net/socket/backend/stub.c
    )

    set(REDASM_NETWORK_LIBS "")
    set(REDASM_HAS_NETWORK FALSE)

    message(STATUS "Network: disabled (${reason})")
endmacro()

option(REDASM_ENABLE_NETWORK "Enable network support" ON)

if(NOT REDASM_ENABLE_NETWORK)
    rd_no_network("REDASM_ENABLE_NETWORK=OFF")
elseif(WIN32)
    set(REDASM_NETWORK_SOURCES 
        src/net/http/backend/winhttp.c
        src/net/socket/backend/stub.c
    )

    set(REDASM_NETWORK_LIBS winhttp)
    set(REDASM_HAS_NETWORK TRUE)

    message(STATUS "Network: WinHTTP")
else()
    find_package(CURL)

    if(CURL_FOUND)
        set(REDASM_NETWORK_SOURCES 
            src/net/socket/backend/posix.c
            src/net/http/backend/curl.c
        )

        set(REDASM_NETWORK_LIBS CURL::libcurl)
        set(REDASM_HAS_NETWORK TRUE)
        message(STATUS "Network: libcurl")
    else()
        rd_no_network("libcurl not found")
    endif()
endif()
