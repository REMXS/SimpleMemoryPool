cmake_minimum_required(VERSION 3.10.0)
project(MemoryPool VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(TEST_DIR ${PROJECT_SOURCE_DIR}/tests)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(/usr/local/include)

find_library(GTEST gtest PATHS /usr/local/lib)
find_library(GTEST_MAIN gtest_main PATHS /usr/local/lib)

add_executable(UnitTest ${SRC} ${TEST_DIR}/unit_test.cpp)
add_executable(PerformanceTest ${SRC} ${TEST_DIR}/performance_test.cpp)

target_link_libraries(UnitTest ${GTEST} ${GTEST_MAIN})

enable_testing()

add_test(
    NAME UnitTest1
    COMMAND $<TARGET_FILE:UnitTest>
)






