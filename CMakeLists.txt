cmake_minimum_required(VERSION 3.16)
project(GoGame LANGUAGES CXX)

# ==========================
# C++ standard
# ==========================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================
# Directories (matching your Makefile)
# ==========================
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INC_DIR ${CMAKE_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

# Put the executable into bin/ (like your Makefile)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ==========================
# Source files: all .cpp in src/
# ==========================
file(GLOB SRC_FILES
    ${SRC_DIR}/*.cpp
)

# Executable name = "main" (same as your Makefile EXECUTABLE := main)
add_executable(main ${SRC_FILES})

# ==========================
# Compiler flags
# (we're now using MinGW / g++, not MSVC)
# ==========================
if (MSVC)
    # Just in case you ever use MSVC again
    target_compile_options(main PRIVATE /W4 /EHsc)
else()
    # MinGW / GCC / Clang
    target_compile_options(main PRIVATE -Wall -Wextra -Wno-unused-parameter -O2)
endif()

# ==========================
# SFML 3.0.0 (MinGW build)
# Mirror your Makefile:
#   SFML_INC := C:/SFML-3.0.0/include
#   SFML_LIB := C:/SFML-3.0.0/lib
# ==========================
set(SFML_ROOT "C:/SFML-3.0.0")
set(SFML_INCLUDE_DIR "${SFML_ROOT}/include")
set(SFML_LIBRARY_DIR "${SFML_ROOT}/lib")

# Include directories
target_include_directories(main
    PRIVATE
        ${INC_DIR}
        ${SFML_INCLUDE_DIR}
)

# Library search path
target_link_directories(main
    PRIVATE
        ${SFML_LIBRARY_DIR}
        ${LIB_DIR}      # in case you add your own libs later
)

# Link SFML libs (exactly like your Makefile LIBRARIES)
target_link_libraries(main
    PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
        sfml-audio
)
