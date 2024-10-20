cmake_minimum_required(VERSION 3.10)
project(MyProject VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(fmt QUIET)
if(NOT fmt_FOUND)
    #message(WARNING "fmt library not found! Switching to C++20")
    set(CMAKE_CXX_STANDARD 20)
endif()

# Option to choose compiler
option(USE_CLANG "Use Clang compiler" OFF)

if(USE_CLANG)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # Static libraries (e.g., libmylib.a)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # Shared libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # Executables (e.g., tests)

# Define source files
set(LIB_SOURCES
    src/debuglog_main.cpp
    src/debugresolve.cpp
    src/debugwatch.cpp
    src/objlog.cpp
    src/tostr_handler.cpp
)

# Define test sources
set(TEST_SOURCES
    tests/main.cpp
    tests/test_tostr.cpp
)

# Create library
add_library(mylib STATIC ${LIB_SOURCES})

# Set include directories for the library
target_include_directories(mylib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mylib
    PUBLIC
        fmt::fmt
)


#set_target_properties(mylib PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build )

# Create test executable
add_executable(tests ${TEST_SOURCES})

# Link test executable with the library
target_link_libraries(tests
    PRIVATE
        mylib
)

# Set include directories for the test executable
target_include_directories(tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)


#set_target_properties(tests PROPERTIES  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build )

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mylib PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
