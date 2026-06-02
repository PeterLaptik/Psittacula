cmake_minimum_required(VERSION 3.10)
project(PSITTACULA_CLI)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INCLUDE_DIR ./include)
set(SRC_DIR ./src)

add_library(${PSITTACULA_CLI_NAME} STATIC
	${INCLUDE_DIR}/ai_client.cpp
	${INCLUDE_DIR}/ai_client.h
	${INCLUDE_DIR}/clients_factory.h
	${INCLUDE_DIR}/console_writer.h
	${INCLUDE_DIR}/format_util.h
	${INCLUDE_DIR}/working_dir.h
	${SRC_DIR}/ai_client_impl.cpp
	${SRC_DIR}/ai_client_impl.h
	${SRC_DIR}/chunk_completion_processor.cpp
	${SRC_DIR}/chunk_completion_processor.h
	${SRC_DIR}/chunk_processor.h
	${SRC_DIR}/clients_factory.cpp
	${SRC_DIR}/completion_rsp.h
	${SRC_DIR}/console_history.cpp
	${SRC_DIR}/console_history.h
	${SRC_DIR}/console_writer.cpp
	${SRC_DIR}/dialogue_body.cpp
	${SRC_DIR}/dialogue_body.h
	${SRC_DIR}/health_rsp.cpp
	${SRC_DIR}/health_rsp.h
	${SRC_DIR}/history_manager.h
	${SRC_DIR}/http_client.cpp
	${SRC_DIR}/http_client.h
	${SRC_DIR}/message.cpp
	${SRC_DIR}/message.h
	${SRC_DIR}/response_health.h
	${SRC_DIR}/response_readers.cpp
	${SRC_DIR}/response_readers.h
	${SRC_DIR}/sys_command.h
	${SRC_DIR}/default_rule_provider.h
	${SRC_DIR}/default_rule_provider.cpp
	${SRC_DIR}/working_dir.cpp
)

set_target_properties(${PSITTACULA_CLI_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_ARCH_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIB_PATH}
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_RUNTIME_PATH}
)

target_include_directories(${PSITTACULA_CLI_NAME} PRIVATE ${INCLUDE_DIR})
target_include_directories(${PSITTACULA_CLI_NAME} PRIVATE ../${PSITTACULA_TOOLS}/include)
target_include_directories(${PSITTACULA_CLI_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
target_include_directories(${PSITTACULA_CLI_NAME} PRIVATE ${rapidjson_SOURCE_DIR}/include)

target_link_libraries(${PSITTACULA_CLI_NAME} PRIVATE ${PSITTACULA_TOOLS_NAME})
target_link_libraries(${PSITTACULA_CLI_NAME} PRIVATE CURL::libcurl)
target_link_libraries(${PSITTACULA_CLI_NAME} PRIVATE rapidjson)
