add_library(doip_core STATIC
    src/doip_header.c
    src/doip_message.c
    src/doip_payload.c
    src/doip_routing_activation_request.c
    src/doip_routing_activation_response.c
)

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
