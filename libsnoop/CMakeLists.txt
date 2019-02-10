cmake_minimum_required(VERSION 3.0)

project(snoop VERSION 1.0.0 DESCRIPTION "function instrumentation")

find_package(Threads REQUIRED)

file(GLOB SOURCES *.cc)

add_definitions(-std=c++11)
add_library(snoop SHARED ${SOURCES})

add_executable(tracer tracer.cc)
target_link_libraries(tracer snoop)

target_link_libraries(snoop ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(snoop PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)
