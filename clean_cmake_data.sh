#!/bin/sh
# ----------------------------------------------------------------
# Full clean all CMake data files
# ----------------------------------------------------------------

DIR_PROJECTS_PATHS="./ ./psittacula-cli/ ./psittacula-tools/ ./psittacula-tui/"

DIR_CMAKEFILES_DIR="CMakeFiles"
for dir in ${DIR_PROJECTS_PATHS}; 
do
  echo "Removing: ${dir}${DIR_CMAKEFILES_DIR}"
  rm -f -r "${dir}${DIR_CMAKEFILES_DIR}"
done

ROOT_FILES="CMakeCache.txt cmake_install.cmake Build Makefile CppProperties.json"
for dir in ${DIR_PROJECTS_PATHS}; 
do
	for file in ${ROOT_FILES};
	do
	  echo "Removing: ${dir}${file}"
	  rm -f "${dir}${file}"
	done
done