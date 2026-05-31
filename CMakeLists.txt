cmake_minimum_required(VERSION 3.25)

project(Psittacula)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Project output paths: %root_project_dir%/Build/*
set(OUTPUT_DIR_NAME Build/${CMAKE_BUILD_TYPE})
set(OUTPUT_DIR_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${OUTPUT_DIR_NAME}) 
set(OUTPUT_RUNTIME_PATH ${OUTPUT_DIR_PATH}/bin)
set(OUTPUT_LIB_PATH ${OUTPUT_DIR_PATH}/lib)
set(OUTPUT_ARCH_PATH ${OUTPUT_DIR_PATH}/lib)
set(OUTPUT_SHARE_PATH ${OUTPUT_DIR_PATH}/share)
set(PACKAGE_PATH ${OUTPUT_DIR_PATH}/package)
message(STATUS "Project build dir path: ./${OUTPUT_DIR_PATH}")

# Additional local modules-helpers
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include(FsUtils)    # reading variables from text files

# Project settings files
set(PROJECT_VERSION_FILENAME version.txt)

# Read settings files
read_variables_from_file(${PROJECT_VERSION_FILENAME})
set(A_VERSION ${A_VERSION_MAJOR}.${A_VERSION_MINOR}.${A_VERSION_PATCH})
message(STATUS "Project version: ${A_VERSION}")

# Dependencies
message(STATUS "Searching dependencies in ${CMAKE_PREFIX_PATH}")

find_package(ZLIB CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(RapidJSON REQUIRED)

# Project contents
# Sub-projects names
set(PSITTACULA_TUI psittacula-tui)
set(PSITTACULA_CLI psittacula-cli)
set(PSITTACULA_TOOLS psittacula-tools)

# Output file names
set(PSITTACULA_TUI_NAME psittacula-${A_VERSION})
set(PSITTACULA_CLI_NAME psittacula-cli-${A_VERSION})
set(PSITTACULA_TOOLS_NAME psittacula-tools-${A_VERSION})

# Sub projects
add_subdirectory(${PSITTACULA_TUI})
add_subdirectory(${PSITTACULA_CLI})
add_subdirectory(${PSITTACULA_TOOLS})

