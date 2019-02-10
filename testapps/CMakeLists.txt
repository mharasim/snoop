cmake_minimum_required(VERSION 3.0)

project(snoop VERSION 1.0.0 DESCRIPTION "test applications")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/testapps)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/testapps)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/testapps)

find_package(Threads REQUIRED)

add_definitions(-finstrument-functions)
add_definitions(-std=c++11)

add_executable(test_1 test_1.cc)
target_link_libraries(test_1 ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_2 test_2.cc)

add_library(test1 SHARED libtest1.cc)
set_target_properties(test1 PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)

add_executable(test_dlopen test_dlopen.cc)
target_link_libraries(test_dlopen test1 ${CMAKE_DL_LIBS})

configure_file(${CMAKE_BINARY_DIR}/scripts/run.sh ${CMAKE_BINARY_DIR}/out/testapps/run.sh COPYONLY)
configure_file(${CMAKE_BINARY_DIR}/scripts/clean.sh ${CMAKE_BINARY_DIR}/out/testapps/clean.sh COPYONLY)

