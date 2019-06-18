project(test_coalesced)
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

function(GENERATE_IDE_FOLDERS SRC_PATH SRC_LIST)
  set( _SRC_LIST ${SRC_LIST} ${ARGN} )
  foreach(_source IN ITEMS ${_SRC_LIST})
    get_filename_component(_source_path "${_source}" REALPATH)
    get_filename_component(_source_path "${_source_path}" PATH)
    file(RELATIVE_PATH _source_path "${SRC_PATH}" "${_source_path}")
    string(REPLACE "/" "\\" _source_path "${_source_path}")
    string(REPLACE "..\\" "" _source_path "${_source_path}")
    source_group("${_source_path}" FILES "${_source}")
  endforeach()
endfunction(GENERATE_IDE_FOLDERS)

include(conan.cmake)
conan_cmake_run(CONANFILE conanfile.txt BASIC_SETUP CMAKE_TARGETS)

set(TEST_SOURCES
  coalesced_test.cpp
  coalesced_hashtable.hpp
)

generate_ide_folders(${PROJECT_SOURCE_DIR}/.. ${TEST_SOURCES})

add_executable(${CMAKE_PROJECT_NAME} ${TEST_SOURCES})
target_link_libraries(${CMAKE_PROJECT_NAME} CONAN_PKG::gtest)
