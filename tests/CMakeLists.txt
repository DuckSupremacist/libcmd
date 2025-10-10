# ―――――――――――――――― Fetch GTest ―――――――――――――――― #

include(FetchContent)

set(GTEST_TAG v1.17.0)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG ${GTEST_TAG}
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# ―――――――――――――――― Build the tests ―――――――――――――――― #

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

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
if(NUM_TESTS
   EQUAL
   0
)
    message(STATUS "No test sources found in ${CMAKE_CURRENT_SOURCE_DIR}/src; skipping test target generation.")
    return()
endif()

if(NUM_TESTED
   GREATER
   0
)
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

add_executable(
    test-all
    ${COMMON_OBJECTS}
    ${TEST_SOURCES}
)

target_link_libraries(
    test-all
    PRIVATE GTest::gtest_main
            ${LIBRARIES}
)

# Register tests
gtest_discover_tests(test-all)
