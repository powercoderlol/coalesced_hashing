project(coalesced)
cmake_minimum_required(VERSION 3.10)

set(SOURCES
  main.cpp
  coalesced_hashtable.hpp
)

add_executable(coalesced ${SOURCES})
