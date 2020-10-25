cmake_minimum_required (VERSION 3.10)

project ("virtual_nes")

# Add the cmake folder so the FindSphinx module is found
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

add_subdirectory ("virtual-nes")
add_subdirectory ("docs")