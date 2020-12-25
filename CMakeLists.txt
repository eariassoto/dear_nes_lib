cmake_minimum_required (VERSION 3.10)

project ("dear_nes_lib")

# Add the cmake folder so the FindSphinx module is found
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

add_subdirectory ("dear_nes_lib")

set(BUILD_DOC FALSE CACHE BOOL "Build documentation with Doxygen and Sphynx")

if(BUILD_DOC)
    add_subdirectory ("docs")
endif()
