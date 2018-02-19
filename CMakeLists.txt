cmake_minimum_required(VERSION 3.1)
project(Espresso)

find_package(CDSS REQUIRED)

set (EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(SRC_FILES
  espresso.c)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set(CMAKE_STATIC_LINKER_FLAGS_DEBUG  "${CMAKE_STATIC_LINKER_FLAGS_DEBUG} -fsanitize=address")

include_directories(SYSTEM ${CDSS_INCLUDE_DIRS})

add_executable(espresso ${SRC_FILES})
target_link_libraries(espresso ${CDSS_LIBRARIES_STATIC})
