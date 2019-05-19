cmake_minimum_required(VERSION 3.5)

project(bhcd LANGUAGES C)

execute_process(COMMAND git describe --always
    OUTPUT_VARIABLE COMMIT_HASH)
configure_file(
  "${PROJECT_SOURCE_DIR}/bhcd/git_version.h.in"
  "${PROJECT_BINARY_DIR}/git_version.h"
)

find_package(GSL REQUIRED)
include_directories(${GSL_INCLUDE_DIRS})

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/bhcd)
include_directories(${PROJECT_SOURCE_DIR}/hccd)
add_subdirectory(bhcd)
add_subdirectory(hccd)

