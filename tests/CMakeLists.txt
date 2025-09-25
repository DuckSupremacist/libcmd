include(FetchContent)

# Enable testing
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP
        TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# ######################################################################################################################

set(CMAKE_CXX_STANDARD 23)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}
)

message("gtest_SOURCE_DIR: ${gtest_SOURCE_DIR}")
message("gtest_INCLUDE_DIRS: ${gtest_INCLUDE_DIRS}")

set(LIBRARIES stdc++fs)

file(
    GLOB_RECURSE
    TESTED_SOURCES
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)
list(
    FILTER
    TESTED_SOURCES
    EXCLUDE
    REGEX
    ".*/main\\.cpp$"
)
list(
    LENGTH
    TESTED_SOURCES
    NUM_TESTED
)

file(
    GLOB_RECURSE
    TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.test.cpp"
)
list(
    LENGTH
    TEST_SOURCES
    NUM_TESTS
)
if(_NUM_TESTS
   EQUAL
   0
)
    message(STATUS "No test sources found in ${CMAKE_CURRENT_SOURCE_DIR}/src; skipping test target generation.")
    return()
endif()

if(_NUM_TESTED
   GREATER
   0
)
    # Compile common sources to object files
    add_library(
        common_objects
        OBJECT
        ${TESTED_SOURCES}
    )
    target_link_libraries(common_objects PRIVATE ${LIBRARIES})
    set(COMMON_OBJECTS $<TARGET_OBJECTS:common_objects>)
else()
    message(STATUS "No implementation sources to compile into common_objects.")
endif()

link_libraries(
    gtest
    gtest_main
    ${LIBRARIES}
)

add_executable(
    test-all
    ${COMMON_OBJECTS}
    ${TEST_SOURCES}
)

# Add the tests to be run
gtest_discover_tests(test-all)
