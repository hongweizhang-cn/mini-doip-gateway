add_library(doip_core STATIC
    src/doip_header.c
    src/doip_gateway.c
    src/doip_message.c
    src/doip_payload.c
    src/doip_routing_activation_request.c
    src/doip_routing_activation.c
    src/doip_routing_activation_state.c
    src/doip_routing_activation_response.c
)

if(WIN32)
    target_sources(doip_core PRIVATE
        src/doip_tcp_server.c
        src/doip_tester.c
    )
    target_link_libraries(doip_core PUBLIC ws2_32)

    add_executable(doip_tcp_server
        src/doip_tcp_server_main.c
    )
    target_link_libraries(doip_tcp_server PRIVATE doip_core)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(doip_tcp_server PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()

    add_executable(doip_tester
        src/doip_tester_main.c
    )
    target_link_libraries(doip_tester PRIVATE doip_core)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(doip_tester PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
endif()

target_include_directories(doip_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(doip_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
