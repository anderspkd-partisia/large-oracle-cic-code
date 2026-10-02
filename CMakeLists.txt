cmake_minimum_required(VERSION 3.5)
project( ks VERSION 0.0.1 DESCRIPTION "Robust ECDSA Threshold Signing at Scale" )

set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD 20)

option(ADDR_SAN
  "enable -fsanitize=address in debug build"
  OFF)

if(CMAKE_BUILD_TYPE MATCHES Debug OR
    CMAKE_BUILD_TYPE MATCHES DebugWithRelInfo)

  if (ADDR_SAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
  endif()

endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(common)

## source file for the dummy broadcast implementation
set(DUMMY_BC
  ${PROJECT_SOURCE_DIR}/common/network.cc)

find_library(GMP gmp libgmp REQUIRED)
find_library(SCL scl REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_subdirectory(experiments)
