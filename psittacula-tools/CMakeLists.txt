cmake_minimum_required(VERSION 3.10)
project(PSITTACULA_TOOLS)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INCLUDE_DIR ./include)
set(CLI_INCLUDE ../${PSITTACULA_CLI}/include)
set(SRC_DIR ./src)

add_library(${PSITTACULA_TOOLS_NAME} STATIC
	${INCLUDE_DIR}/tool_base.h
	${SRC_DIR}/tool_base.cpp
	${INCLUDE_DIR}/tool_factory.h
	${SRC_DIR}/tool_file.h
	${SRC_DIR}/tool_dir_delete.cpp
	${SRC_DIR}/tool_dir_delete.h
	${SRC_DIR}/tool_factory.cpp
	${SRC_DIR}/tool_file_create.cpp
	${SRC_DIR}/tool_file_create.h
	${SRC_DIR}/tool_file_create_binary.cpp
	${SRC_DIR}/tool_file_create_binary.h
	${SRC_DIR}/tool_dir_create.h
	${SRC_DIR}/tool_dir_create.cpp
	${SRC_DIR}/tool_file_remove.h
	${SRC_DIR}/tool_file_remove.cpp
	${SRC_DIR}/tool_file_modify.h
	${SRC_DIR}/tool_file_modify.cpp
	${SRC_DIR}/tool_file_move.cpp
	${SRC_DIR}/tool_file_move.h
	${SRC_DIR}/tool_file_read.cpp
	${SRC_DIR}/tool_file_read.h
	${SRC_DIR}/tool_file_search.cpp
	${SRC_DIR}/tool_file_search.h
	${SRC_DIR}/tool_file_list.cpp
	${SRC_DIR}/tool_file_list.h
	${SRC_DIR}/tool_file_copy.cpp
	${SRC_DIR}/tool_file_copy.h
)

set_target_properties(${PSITTACULA_TOOLS_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_ARCH_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIB_PATH}
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_RUNTIME_PATH}
)

target_include_directories(${PSITTACULA_TOOLS_NAME} PRIVATE ${INCLUDE_DIR})
target_include_directories(${PSITTACULA_TOOLS_NAME} PRIVATE ${CLI_INCLUDE})
target_include_directories(${PSITTACULA_TOOLS_NAME} PRIVATE ${rapidjson_SOURCE_DIR}/include)

target_link_libraries(${PSITTACULA_TOOLS_NAME} PRIVATE rapidjson)
