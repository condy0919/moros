find_package(Boost REQUIRED COMPONENTS unit_test_framework)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${moros_SOURCE_DIR}/bin/tests)

include_directories(${moros_SOURCE_DIR}/src ${Boost_INCLUDE_DIRS})
add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(numfmt numfmt.cpp)
target_link_libraries(numfmt ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_test(NAME numfmt COMMAND numfmt)
