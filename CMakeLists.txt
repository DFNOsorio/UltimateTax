cmake_minimum_required(VERSION 3.21)
project(ultimateTax VERSION 0.1.0 LANGUAGES C)

option(UTAX_BUILD_TESTS "Build unit tests" ON)
option(UTAX_STAGE_DB_ASSETS "Copy DB assets (schema/sample db) into staging folder" ON)

# For clangd/Zed (ignored by Visual Studio generator; works with Ninja/Makefiles)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# install/ is your zip-ready staging folder (default)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/install" CACHE PATH "Staging folder (zip-ready)" FORCE)
endif()

# Keep build artifacts inside build_output/
foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_uc)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_uc} "${CMAKE_BINARY_DIR}/lib/${cfg}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_uc} "${CMAKE_BINARY_DIR}/lib/${cfg}")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_uc} "${CMAKE_BINARY_DIR}/bin/${cfg}")
endforeach()
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---- Library ----
add_library(ultimateTax STATIC
  lib/src/utax_db.c
  lib/src/utax_schema.c
  lib/src/utax_trades.c
  lib/src/utax_dividends.c
  lib/src/utax_options.c
  lib/src/utax_fifo_snapshot.c
  lib/src/utax_fifo_snapshot_action_applied.c
  lib/src/utax_fifo_realized.c
  lib/src/utax_corporate_actions.c
  lib/src/utax_process_year.c
  lib/src/utax_utils.c
  vendor/sqlite/sqlite3.c
)

add_library(ultimateTax::ultimateTax ALIAS ultimateTax)



target_include_directories(ultimateTax
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/sqlite
    ${CMAKE_SOURCE_DIR}/lib/src
)

# Keep sqlite amalgamation warnings quiet
if(MSVC)
    set_source_files_properties(vendor/sqlite/sqlite3.c PROPERTIES COMPILE_OPTIONS "/W0")
    target_compile_options(ultimateTax PRIVATE /W4 /permissive-)
else()
    set_source_files_properties(vendor/sqlite/sqlite3.c PROPERTIES COMPILE_OPTIONS "-w")
    target_compile_options(ultimateTax PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- Tests ----
if(UTAX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- Staging rules (zip-ready layout under ${CMAKE_INSTALL_PREFIX}) ----
# Uses $<CONFIG> so multi-config generators stage per-config without duplicating exports.
install(TARGETS ultimateTax
  EXPORT ultimateTaxTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/$<CONFIG>
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/$<CONFIG>
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/lib/include/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

# Ship schema.sql (and optionally portfolio.db) in the staged zip
if(UTAX_STAGE_DB_ASSETS)
    install(FILES
    "${CMAKE_SOURCE_DIR}/db/portfolio/schema.sql"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/ultimateTax/db"
  )

    # Optional: include a sample DB in the zip (uncomment if you want it shipped)
    # install(FILES
    #   "${CMAKE_SOURCE_DIR}/db/portfolio/portfolio.db"
    #   DESTINATION "${CMAKE_INSTALL_DATADIR}/ultimateTax/db"
    # )
endif()

# Optional: CMake package files for consumers using find_package(ultimateTax CONFIG)
install(EXPORT ultimateTaxTargets
  NAMESPACE ultimateTax::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ultimateTax
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
  "${CMAKE_SOURCE_DIR}/cmake/ultimateTaxConfig.cmake.in"
  "${CMAKE_BINARY_DIR}/ultimateTaxConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ultimateTax"
)
write_basic_package_version_file(
  "${CMAKE_BINARY_DIR}/ultimateTaxConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${CMAKE_BINARY_DIR}/ultimateTaxConfig.cmake"
  "${CMAKE_BINARY_DIR}/ultimateTaxConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ultimateTax"
)

# ---- A build target that stages into install/ (zip-ready) ----
if(CMAKE_CONFIGURATION_TYPES)
    add_custom_target(utax_stage
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${CMAKE_INSTALL_PREFIX}" --config $<CONFIG>
    COMMENT "Staging ultimateTax into: ${CMAKE_INSTALL_PREFIX}"
    VERBATIM
  )
else()
    add_custom_target(utax_stage
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${CMAKE_INSTALL_PREFIX}"
    COMMENT "Staging ultimateTax into: ${CMAKE_INSTALL_PREFIX}"
    VERBATIM
  )
endif()
