# ―――――――――――――――― Configure CMake ―――――――――――――――― #
cmake_minimum_required(VERSION 3.22)

set(CMAKE_C_COMPILER /usr/bin/gcc-14)
set(CMAKE_CXX_COMPILER /usr/bin/g++-14)

set(PROJECT_NAME libcmd)
project(
    ${PROJECT_NAME}
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

# ―――――――――――――――― Build the Project ―――――――――――――――― #

set(EXEC_NAME example)

file(
    GLOB_RECURSE
    SOURCE_FILES
    "src/*.cpp"
)

add_executable(${EXEC_NAME} ${SOURCE_FILES})

target_include_directories(
    ${EXEC_NAME}
    PRIVATE ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_BINARY_DIR}/include
)

target_compile_options(
    ${EXEC_NAME}
    PUBLIC -O3
           -Wall
           -Woverloaded-virtual
           -Wno-unused-function
           -Wmissing-declarations
           -Werror
)

# ―――――――――――――――― Build the tests ―――――――――――――――― #

option(
    BUILD_TESTS
    "Build the tests"
    ON
)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
