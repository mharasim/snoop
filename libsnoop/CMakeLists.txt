cmake_minimum_required(VERSION 3.0)

project(snoop VERSION 1.0.0)

find_package(Threads REQUIRED)

file(GLOB SOURCES *.cc)

if(SNOOP_LOG_DISABLED)
    add_definitions(-DSNOOP_LOG_DISABLED)
endif()
if(SNOOP_LOG_LEVEL)
    add_definitions(-DSNOOP_LOG_LEVEL=${SNOOP_LOG_LEVEL})
endif()
if(SNOOP_SPAWN_TRACER)
    add_definitions(-DSNOOP_SPAWN_TRACER)
endif()
if(SNOOP_TRACER_USE_EXECVE)
    add_definitions(-DSNOOP_TRACER_USE_EXECVE)
endif()

add_definitions(-std=c++11)
add_library(snoop SHARED ${SOURCES})

add_executable(tracer tracer.cc)
target_link_libraries(tracer snoop)

target_link_libraries(snoop ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(snoop PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)
