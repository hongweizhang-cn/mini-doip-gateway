add_executable(test_doip_header
    test_doip_header.c
)

target_link_libraries(test_doip_header PRIVATE doip_core)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_doip_header PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

add_test(NAME test_doip_header COMMAND test_doip_header)
