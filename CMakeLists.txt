cmake_minimum_required(VERSION 3.10)
project(MyProject VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # Static libraries (e.g., libmylib.a)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   # Shared libraries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)   # Executables (e.g., tests)

# Load FMT
find_package(fmt QUIET)
if(fmt_FOUND)
    message(STATUS "Fmt library used.")
else()
    message(WARNING "fmt library not found! Enforce C++20 for std::format")
    set(CMAKE_CXX_STANDARD 20)
endif()

# Option to choose compiler
option(USE_CLANG "Use Clang compiler" OFF)

if(USE_CLANG)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
endif()


option(PREVENT_MAGIC_ENUM "Disable magic_enum library" OFF)
option(PREVENT_REFLECT_ENUM "Disable qlibs/reflect library" OFF)


include(FetchContent)

#GREATER_EQUAL
if (CMAKE_CXX_STANDARD LESS 20)
    set(PREVENT_REFLECT_ENUM ON)
endif()

if (NOT PREVENT_REFLECT_ENUM)
    message(STATUS "Attempting to download qlibs/reflect...")

    FetchContent_Declare(
        qlibs_reflect
        GIT_REPOSITORY https://github.com/qlibs/reflect
        GIT_TAG main 
    )

    FetchContent_MakeAvailable(qlibs_reflect)

    if (qlibs_reflect_POPULATED)
        message(STATUS "Using qlibs/reflect.")
        set(PREVENT_REFLECT_ENUM ON)
    else()
        message(WARNING "qlibs/reflect not found. Falling back to magic_enum.")
        #add_definitions(-DPREVENT_REFLECT_ENUM)
        set(PREVENT_MAGIC_ENUM ON)
    endif()
endif()


if(PREVENT_REFLECT_ENUM AND NOT PREVENT_MAGIC_ENUM)
    message(STATUS "Attempting to download magic_enum...")

    FetchContent_Declare(
        magic_enum
        GIT_REPOSITORY https://github.com/Neargye/magic_enum
        GIT_TAG master
    )
    FetchContent_MakeAvailable(magic_enum)

    if (magic_enum_POPULATED)
        message(STATUS "Using magic_enum.")
    else()
        message(WARNING "magic_enum not found. Falling back to magic_enum.")
        #add_definitions(-DPREVENT_MAGIC_ENUM)
        set(PREVENT_MAGIC_ENUM ON)
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
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/
)

if (NOT PREVENT_REFLECT_ENUM)
        target_include_directories(mylib PRIVATE ${qlibs_reflect_SOURCE_DIR}/include)
        target_include_directories(tests PRIVATE ${qlibs_reflect_SOURCE_DIR}/include)
endif()
if (NOT PREVENT_MAGIC_ENUM)
        target_include_directories(mylib PRIVATE ${magic_enum_SOURCE_DIR}/include)
        target_include_directories(tests PRIVATE ${magic_enum_SOURCE_DIR}/include)
endif()


#set_target_properties(tests PROPERTIES  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build )

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mylib PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
