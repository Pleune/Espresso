cmake_minimum_required(VERSION 3.1)
project(Espresso)

find_package(CDSS REQUIRED)

set (EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(SRC_FILES
  espresso.c)

include_directories(SYSTEM ${CDSS_INCLUDE_DIRS})

add_executable(espresso ${SRC_FILES})
target_link_libraries(espresso ${CDSS_LIBRARIES_STATIC})
