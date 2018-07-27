cmake_minimum_required(VERSION 3.0)

project(wfc_richest)

set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -std=c++11")

set(BINDIR "${CMAKE_BINARY_DIR}/bin")
set(LIBDIR "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BINDIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${LIBDIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${LIBDIR}")

execute_process(COMMAND "jsonrpcstub" ${CMAKE_SOURCE_DIR}/client.json --cpp-client=wfcClient --cpp-client-file=wfcclient.h -v)

include_directories(${CMAKE_BINARY_DIR})

add_executable(wfc_richest
sqlitedb.h
wfcclient.h
WfcHelper.h
WfcHelper.cpp
threadpool.cpp
main.cpp
)

target_link_libraries(wfc_richest
boost_thread
boost_system
sqlite3
pthread
jsonrpccpp-common
jsonrpccpp-client
jsoncpp
)
