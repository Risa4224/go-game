cmake_minimum_required(VERSION 3.16)
project(GoGame LANGUAGES CXX)

# ==========================
# C++ standard
# ==========================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================
# Directories Configuration
# ==========================
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(INC_DIR ${CMAKE_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

# Xuất file .exe ra folder bin/ nằm ngay thư mục gốc dự án
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ==========================
# SFML PORTABLE CONFIGURATION
# (Thay đổi: Trỏ vào folder sfml nằm trong dự án thay vì ổ D:)
# ==========================
set(SFML_ROOT "${CMAKE_SOURCE_DIR}/sfml")
set(SFML_INCLUDE_DIR "${SFML_ROOT}/include")
set(SFML_LIBRARY_DIR "${SFML_ROOT}/lib")
set(SFML_BIN_DIR     "${SFML_ROOT}/bin") # Nơi chứa DLL gốc để copy

# ==========================
# Source files
# ==========================
file(GLOB SRC_FILES
    ${SRC_DIR}/*.cpp
)

add_executable(main ${SRC_FILES})

# ==========================
# Compiler flags
# ==========================
if (MSVC)
    target_compile_options(main PRIVATE /W4 /EHsc)
else()
    # MinGW / GCC / Clang
    target_compile_options(main PRIVATE -Wall -Wextra -Wno-unused-parameter -O2)
endif()

# ==========================
# Link & Include Directories
# ==========================
target_include_directories(main
    PRIVATE
        ${INC_DIR}
        ${SFML_INCLUDE_DIR}
)

target_link_directories(main
    PRIVATE
        ${SFML_LIBRARY_DIR}
        ${LIB_DIR}
)

# Link SFML libs
target_link_libraries(main
    PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
        sfml-audio
)

# ==========================
# AUTO COPY DLLs (Quan trọng)
# ==========================
# Sau khi build xong main.exe, tự động copy DLL từ sfml/bin sang bin/
add_custom_command(TARGET main POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${SFML_BIN_DIR}"
    "$<TARGET_FILE_DIR:main>"
    COMMENT "Dang copy SFML DLLs tu sfml/bin sang folder output..."
)