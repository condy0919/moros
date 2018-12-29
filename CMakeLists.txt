cmake_minimum_required(VERSION 3.0.2)

project(moros)

enable_testing()

include(ExternalProject)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${moros_SOURCE_DIR}/bin)

add_custom_target(third_party)
ExternalProject_Add(
    http_parser
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/http-parser
    PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/third_party/http-parser
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE TRUE
    BUILD_COMMAND make package
    INSTALL_COMMAND ""
)

add_dependencies(third_party http_parser)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/http-parser)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/http-parser)

add_subdirectory(src)
add_subdirectory(tests)
