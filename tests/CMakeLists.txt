# tests/CMakeLists.txt

# Helper to declare a test executable and register it in CTest
function(utax_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE ultimateTax)

    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME ${name} COMMAND ${name})
endfunction()

utax_add_test(utax_test_db_open
  test_db_open.c
)
