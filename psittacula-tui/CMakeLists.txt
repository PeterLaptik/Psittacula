cmake_minimum_required(VERSION 3.25)
project(${PSITTACULA_TUI})

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(SRC_DIR ./src)
set(INCLUDE_DIR ./include)
set(CLI_INCLUDE ../${PSITTACULA_CLI}/include)
set(TOOLS_INCLUDE ../${PSITTACULA_TOOLS}/include)

add_executable(${PSITTACULA_TUI_NAME}
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/app.cpp
    ${SRC_DIR}/app.h
    ${SRC_DIR}/chat_command.h
    ${SRC_DIR}/chat_command_dispatcher.h
    ${SRC_DIR}/command_change_agent.h
    ${SRC_DIR}/command_change_model.h
    ${SRC_DIR}/command_change_project_dir.h
    ${SRC_DIR}/command_clean_context.h
    ${SRC_DIR}/command_compress.h
    ${SRC_DIR}/command_dump.h
    ${SRC_DIR}/command_dump_text.h
    ${SRC_DIR}/command_reasoning.h
    ${SRC_DIR}/command_redo.h
    ${SRC_DIR}/command_restore.h
    ${SRC_DIR}/command_tools.h
    ${SRC_DIR}/command_undo.h
    ${SRC_DIR}/exit_dialog.cpp
    ${SRC_DIR}/exit_dialog.h
    ${SRC_DIR}/init_commands.cpp
    ${SRC_DIR}/init_commands.h
    ${SRC_DIR}/input_text_content.cpp
    ${SRC_DIR}/input_text_content.h
    ${SRC_DIR}/keyboard.cpp
    ${SRC_DIR}/keyboard.h
    ${SRC_DIR}/logo.h
    ${SRC_DIR}/model.cpp
    ${SRC_DIR}/model.h
    ${SRC_DIR}/panel.cpp
    ${SRC_DIR}/panel.h
    ${SRC_DIR}/panel_input.cpp
    ${SRC_DIR}/panel_input.h
    ${SRC_DIR}/panel_text.cpp
    ${SRC_DIR}/panel_text.h
    ${SRC_DIR}/param_reader.cpp
    ${SRC_DIR}/param_reader.h
    ${SRC_DIR}/response.h
    ${SRC_DIR}/screen.cpp
    ${SRC_DIR}/screen.h
    ${SRC_DIR}/screen_text_content.cpp
    ${SRC_DIR}/screen_text_content.h
    ${SRC_DIR}/text_splitter.cpp
    ${SRC_DIR}/text_splitter.h
)

target_compile_definitions(${PSITTACULA_TUI_NAME} PRIVATE PSITTACULA_APP_VERSION="${A_VERSION}")

set_target_properties(${PSITTACULA_TUI_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${OUTPUT_ARCH_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_LIB_PATH}
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_RUNTIME_PATH}
)

target_link_directories(${PSITTACULA_TUI_NAME} PRIVATE ${OUTPUT_LIB_PATH})

target_include_directories(${PSITTACULA_TUI_NAME} PRIVATE ${INCLUDE_DIR})
target_include_directories(${PSITTACULA_TUI_NAME} PRIVATE ${CLI_INCLUDE})
target_include_directories(${PSITTACULA_TUI_NAME} PRIVATE ${TOOLS_INCLUDE})

target_link_libraries(${PSITTACULA_TUI_NAME} PRIVATE ${PSITTACULA_CLI_NAME})
target_link_libraries(${PSITTACULA_TUI_NAME} PRIVATE ${PSITTACULA_TOOLS_NAME})