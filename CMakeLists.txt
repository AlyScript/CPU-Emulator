cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to find an appropriate clang-tidy somewhere
find_program(TIDY NAMES clang-tidy-17 clang-tidy-18 clang-tidy-16 clang-tidy-15 clang-tidy-14 vclang-tidy-13 clang-tidy-12 clang-tidy)

project(Emulator VERSION 0.5 LANGUAGES CXX)

#-------------------------------------------------------------------------------
#------------------------------        FLAGS      ------------------------------
#-------------------------------------------------------------------------------
# In a realistic setup, we wouldn't have compiler flags hardcoded in CMakeLists.txt
# but in a separate CMakePresets.json file as presets selectable by users.
# But for this simple project, hardcoded flags are okay.

# Flags to use to compile and link the emulator
if (MSVC)
	# These flags are used when using the Visual Studio Compiler.
	# We don't really support the Visual Studio Compiler, so these flags are
	#   provided with little testing and they might produce different results
	#   than the GCC/Clang flags.
	# Before submission, test your work on a lab machine using GCC.
	# -Od : no optimizations
	# -W4 : report almost all warnings
	# -Zi : produce debug inormation
	set(MYFLAGS "-Od;-Zi;-W4")
else()
	# These flags are used with all other compilers, assumming that all other compilers are GCC or Clang
	# -Og : apply only optimizations that don't make debugging hard
	# -g : include debug information in the executable
	# -Wall : report all standard warnings
	# -Wextra: report a selection of non-standard warnings
	# -Wpedantic: report warnings that a bit less important
	set(MYFLAGS "-Og;-g;-Wall;-Wextra;-Wpedantic")
endif()

# Project defaults:
# - Disable all compilation warnings as the default
# - Disable optimizations (for fast compilation)
if (MSVC)
	add_compile_options("-Od;-w")
else()
	add_compile_options("-O0;-w")
endif()

# Everything is linked using the flags above
add_link_options(${MYFLAGS})

#-------------------------------------------------------------------------------
#------------------------------     COMPONENTS    ------------------------------
#------------------------   to be compiled separately   ------------------------ 
#-------------------------------------------------------------------------------

# Create a separate emulator "library" from the part of the project modified by students
add_library(emulator STATIC emulator.cpp instructions.cpp)
target_compile_options(emulator PRIVATE ${MYFLAGS})

# Create another emulator library from the same source files, but with the address sanitizer enabled
add_library(emulator_asan STATIC emulator.cpp instructions.cpp)
target_compile_options(emulator_asan PRIVATE ${MYFLAGS} "-fsanitize=address")

# We pre-compile catch separately to improve compilation speed
add_library(catch STATIC catch.cpp)

#-------------------------------------------------------------------------------
#------------------------------    EXECUTABLES    ------------------------------
#-------------------------------------------------------------------------------

# 1. The structural tests
add_executable(structural-tests structural-tests.cpp)
target_link_libraries(structural-tests emulator catch)

# 2. The functional tests
add_executable(functional-tests functional-tests.cpp)
target_link_libraries(functional-tests emulator catch)

# 3. The functional tests with address sanitization
if(MSVC)
	#MSVC doesn't like incremental builds with the address sanitizer
	add_executable(sanitized-tests functional-tests.cpp catch.cpp emulator.cpp instructions.cpp)
	target_compile_options(sanitized-tests PUBLIC ${MYFLAGS} "-fsanitize=address")
else()
	add_executable(sanitized-tests functional-tests.cpp)
	target_link_libraries(sanitized-tests emulator_asan catch)
	target_link_options(sanitized-tests PUBLIC "-fsanitize=address")
endif()

#-------------------------------------------------------------------------------
#------------------------------      ACTIONS      ------------------------------
#-------------------------------------------------------------------------------

# 1. tests -> Run both structural and functional tests
add_custom_target(
	tests
	COMMAND structural-tests
	COMMAND functional-tests
	USES_TERMINAL
	COMMENT "Trying the tests"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# 2. sanitized -> Run the Address Sanitizer
add_custom_target(
	sanitized
	COMMAND sanitized-tests
	USES_TERMINAL
	COMMENT "Trying the tests with the Address Sanitizer enabled"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# 3. tidy -> Run clang-tidy or MSVC on the emulator code
if (MSVC)
	message("Use the C++ Core Guidelines Checker embedded in your Visual Studio: https://learn.microsoft.com/en-us/cpp/code-quality/using-the-cpp-core-guidelines-checkers?view=msvc-170")
else()
	add_custom_target(
		tidy
		COMMAND ${TIDY} -checks=cppcoreguidelines-*,clang-analyzer-* -header-filter=.* instructions.cpp emulator.cpp -- -O2 -std=c++20
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	)
endif()

#-------------------------------------------------------------------------------
#------------------------------      TESTING      ------------------------------
#-------------------------------------------------------------------------------

# Include the current path in the list of paths we check when importing CMake rules
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR})

# This is the CMake testing suite
include(CTest)

# These are some rules for processing Catch2 tests
include(Catch)

# Register all tests in the two executables with CTest
catch_discover_tests(structural-tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
catch_discover_tests(functional-tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

