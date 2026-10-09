# Engine unit tests: no network, no UI; the only external dependency is
# header-only rapidjson (the CLI lib is deliberately partially compiled to
# avoid libcurl in the test target).
#
# rapidjson discovery, in order:
#   1. find_package(RapidJSON CONFIG)       - vcpkg toolchain / CMAKE_PREFIX_PATH
#   2. $VCPKG_ROOT environment variable     - the common case on this machine
#   3. tools/ dir next to the sources       - legacy in-tree layout
# Configure fails with a clear message if none works.

set(TEST_SOURCES
    test_main.cpp       # the zero-dependency harness entry point
    test_utf8_util.cpp
    test_chunk_processor.cpp
    test_dialogue_body.cpp
    test_dialogue_body_compress.cpp
    test_working_dir_sandbox.cpp
    test_text_splitter.cpp
)

# Engine sources under test (psittacula-cli; http_client.cpp is excluded:
# it would drag libcurl into the test binary)
set(CLI_ENGINE_SOURCES
    ../psittacula-cli/src/dialogue_body.cpp
    ../psittacula-cli/src/console_writer.cpp
    ../psittacula-cli/src/console_history.cpp
    ../psittacula-cli/src/working_dir.cpp
    ../psittacula-cli/src/context_info.cpp
)

# Engine sources under test (psittacula-tui)
set(TUI_ENGINE_SOURCES
    ../psittacula-tui/src/text_splitter.cpp
)

add_executable(tests-psittacula ${TEST_SOURCES} ${CLI_ENGINE_SOURCES} ${TUI_ENGINE_SOURCES})

target_compile_features(tests-psittacula PRIVATE cxx_std_17)

target_include_directories(tests-psittacula PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # src/ before include/: chunk_processor resided in BOTH dirs (the stale
    # include/ copy is removed now, the order is kept as a guard for the future)
    ../psittacula-cli/src
    ../psittacula-cli/include
    ../psittacula-tools/include
    ../psittacula-tui/src          # TextSplitter header lives here
)

# --- rapidjson ---
find_package(RapidJSON CONFIG QUIET)

if(NOT RapidJSON_FOUND)
    if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/installed/x64-windows/include/rapidjson/document.h")
        set(RAPIDJSON_INCLUDE_DIR "$ENV{VCPKG_ROOT}/installed/x64-windows/include")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../tools/rapidjson/include/rapidjson/document.h")
        set(RAPIDJSON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/rapidjson/include")
    endif()
endif()

if(RapidJSON_FOUND)
    # vcpkg's config exports an interface target with include paths
    if(TARGET rapidjson)
        target_link_libraries(tests-psittacula PRIVATE rapidjson)
    elseif(RAPIDJSON_INCLUDE_DIRS)
        target_include_directories(tests-psittacula PRIVATE ${RAPIDJSON_INCLUDE_DIRS})
    endif()
elseif(DEFINED RAPIDJSON_INCLUDE_DIR)
    target_include_directories(tests-psittacula PRIVATE ${RAPIDJSON_INCLUDE_DIR})
    message(STATUS "tests-psittacula: rapidjson headers from ${RAPIDJSON_INCLUDE_DIR}")
else()
    message(FATAL_ERROR
        "rapidjson not found for tests-psittacula.\n"
        "Configure with the vcpkg toolchain (as the main build does) or set VCPKG_ROOT.")
endif()

set_target_properties(tests-psittacula PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_RUNTIME_PATH}
)
