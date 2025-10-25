cmake_minimum_required(VERSION 3.20)

# Option to choose compiler (moved before project)
option(USE_CLANG "Use Clang compiler" OFF)

if(USE_CLANG)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
endif()

project(DebugLog VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # Static libraries (e.g., libdebuglog.a)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # Shared libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # Executables (e.g., tests)

# Load FMT
find_package(fmt QUIET)
if(fmt_FOUND)
    message(STATUS "Fmt library used.")
    add_compile_definitions(FMT_HEADER_ONLY)
else()
    message(WARNING "fmt library not found! Enforce C++20 for std::format")
    set(CMAKE_CXX_STANDARD 20)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles("
        #include <format>
        int main(){ (void)std::format(\"{}\", 1); }
    " HAVE_STD_FORMAT)
    if(HAVE_STD_FORMAT)
        message(STATUS "Using std::format")
    else()
        message(FATAL_ERROR "Neither fmt nor std::format is available")
    endif()
endif()

option(DEBUGLOG_USE_MAGIC_ENUM "Use magic_enum library" ON)
option(DEBUGLOG_USE_REFLECT "Use qlibs/reflect library" ON)

include(FetchContent)

# Disable reflect if C++ < 20
if (CMAKE_CXX_STANDARD LESS 20)
    set(DEBUGLOG_USE_REFLECT OFF)
endif()

if (DEBUGLOG_USE_REFLECT)
    message(STATUS "Attempting to download qlibs/reflect...")

    FetchContent_Declare(
        qlibs_reflect
        GIT_REPOSITORY https://github.com/qlibs/reflect
        # pinned to tag for deterministic build
        GIT_TAG v1.2.6 
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(qlibs_reflect)

    if (qlibs_reflect_POPULATED)
        message(STATUS "Using qlibs/reflect.")
    else()
        message(WARNING "qlibs/reflect not found. Falling back to magic_enum.")
        set(DEBUGLOG_USE_REFLECT OFF)
        set(DEBUGLOG_USE_MAGIC_ENUM ON)
    endif()
endif()

if (NOT DEBUGLOG_USE_REFLECT AND DEBUGLOG_USE_MAGIC_ENUM)
    message(STATUS "Attempting to download magic_enum...")

    FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum
        # pinned to tag for deterministic build
        GIT_TAG v0.9.7
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(magic_enum)

    if (magic_enum_POPULATED)
        message(STATUS "Using magic_enum.")
    else()
        message(WARNING "magic_enum not found.")
        set(DEBUGLOG_USE_MAGIC_ENUM OFF)
    endif()
endif()
   

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
    tests/test_sentry.cpp
    tests/test_sentry_extra.cpp
    tests/test_objlog.cpp
    tests/debuglog_tostr_my_handler.cpp
)

# Create library
add_library(debuglog STATIC ${LIB_SOURCES})

# Set include directories for the library
target_include_directories(debuglog
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(debuglog
    PUBLIC
        fmt::fmt
)


#set_target_properties(debuglog PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build )

# Create test executable
add_executable(tests ${TEST_SOURCES})

# Link test executable with the library
target_link_libraries(tests
    PRIVATE
        debuglog
        fmt::fmt
)
#todo - fmt может и отсутствовать а быть фоллбек на std

# Set include directories for the test executable
target_include_directories(tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/
)

# Propagate include dirs for optional deps
if (DEBUGLOG_USE_REFLECT)
    target_link_libraries(debuglog PUBLIC qlibs_reflect)
endif()
if (DEBUGLOG_USE_MAGIC_ENUM)
    target_link_libraries(debuglog PUBLIC magic_enum)
endif()

#set_target_properties(tests PROPERTIES  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build )

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(debuglog PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
