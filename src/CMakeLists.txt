cmake_minimum_required(VERSION 2.6)

add_definitions(-std=c++14)
add_definitions(-pipe)
add_definitions(-Wall)
add_definitions(-Wextra)
add_definitions(-g -ggdb)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options)
find_package(Git REQUIRED)
find_package(OpenSSL REQUIRED)

# generate version file
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --always --tags --dirty
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE PROJ_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/version.hpp
    "const char* version = __DATE__ \" ${PROJ_VERSION}\";"
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
file(GLOB MOROS_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/stdhack.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/stats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bencher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ssl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_executable(moros ${MOROS_SRC} ${CMAKE_CURRENT_BINARY_DIR}/version.hpp)

add_dependencies(moros third_party)

target_link_libraries(moros
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    libhttp_parser.a
)
