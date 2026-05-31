# Module: FsUtils.cmake
# Author: Peter Laptik
# Description: A set of helper functions for reading data (settings) from text files

# Reads variables from file (KEY=VALUE)
# FILE_PATH - file path
function (read_variables_from_file FILE_PATH)
    file(STRINGS ./${FILE_PATH} PROJECT_VARS)
    message(STATUS "Setting up project variables from the file: '${FILE_PATH}'")
        foreach(LINE ${PROJECT_VARS})
            string(REGEX MATCH "(.*)=(.*)" _ ${LINE})
            set(${CMAKE_MATCH_1} ${CMAKE_MATCH_2} PARENT_SCOPE)
            message(STATUS "\t ${CMAKE_MATCH_1} = ${CMAKE_MATCH_2}")
        endforeach()
endfunction()

# Reads list variables from file (KEY:=VALUE1 VALUE2 VALUE3 ...)
# FILE_PATH - file path
function (read_list_variables_from_file FILE_PATH)
    file(STRINGS ./${FILE_PATH} PROJECT_VARS)
    message(STATUS "Setting up project list variables from the file: '${FILE_PATH}'")
        foreach(LINE ${PROJECT_VARS})
            string(REGEX MATCH "(.*):=(.*)" _ ${LINE})
            string(REPLACE " " ";" STR_TO_LIST ${CMAKE_MATCH_2})
            separate_arguments(CMAKE_MATCH_2)
            set(${CMAKE_MATCH_1} ${CMAKE_MATCH_2} PARENT_SCOPE)
            message(STATUS "\t ${CMAKE_MATCH_1} = ${STR_TO_LIST}")
        endforeach()
endfunction()