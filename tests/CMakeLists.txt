# tests/CMakeLists.txt

function(utax_add_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE ultimateTax)

    # Tests may include <sqlite3.h>
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/vendor/sqlite)

    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive-)
        target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Existing test
utax_add_test(utax_test_db_open
  test_db_open.c
)
add_test(NAME utax_test_db_open COMMAND $<TARGET_FILE:utax_test_db_open>)

# New schema test
utax_add_test(utax_test_schema
  test_schema.c
)

# Pass schema path as argv[1]
add_test(NAME utax_test_schema
  COMMAND $<TARGET_FILE:utax_test_schema> "${CMAKE_SOURCE_DIR}/db/portfolio/schema.sql"
)

utax_add_test(utax_test_trades
  test_trades.c
)

add_test(NAME utax_test_trades
  COMMAND $<TARGET_FILE:utax_test_trades> "${CMAKE_SOURCE_DIR}/db/portfolio/schema.sql"
)

utax_add_test(utax_test_dividends
  test_dividends.c
)

add_test(NAME utax_test_dividends
  COMMAND $<TARGET_FILE:utax_test_dividends> "${CMAKE_SOURCE_DIR}/db/portfolio/schema.sql"
)
