cmake_minimum_required(VERSION 3.5)
project(wtclasses)

enable_testing()

set(VERSION_MAJOR 1)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(SONAME 15)

set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(CMAKE_CXX_STANDARD 20)

set(local_include_dir ${PROJECT_BINARY_DIR}/include)
set(local_headers_dir ${local_include_dir}/Wt/Wc)

set(WT_LIB_SUFFIX "" CACHE STRING "Wt lib suffix (empty or 'd')")

set(datadir ${CMAKE_INSTALL_PREFIX}/share CACHE PATH "Root of data files")
set(WT_DIR ${datadir} CACHE PATH "Wt installation dir")
message("WT_DIR is ${WT_DIR}")
set(DOC_SUBDIR ${datadir}/doc/libwtclasses CACHE PATH "Docs subdir")

include(${CMAKE_CURRENT_SOURCE_DIR}/features/ubuntu-gcc.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/features/Features.cmake)
INCLUDE_DIRECTORIES(${WT_DIR}/include)

LINK_DIRECTORIES(${WT_DIR}/lib)

get_property(propVal DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message("INCLUDE_DIRECTORIES: ${propVal}")

subdirs(src)
file(GLOB local_headers "${local_headers_dir}/*.hpp")

subdirs(locales css)

option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    subdirs(examples)
endif()

option(BUILD_DOC "Build documentation" ON)
if(BUILD_DOC)
    subdirs(doc)
endif()

option(BUILD_LOCALES_TEST "Build locales-test" ON)
if(BUILD_LOCALES_TEST)
    subdirs(locales-test)
endif()
