project(coalesced)
cmake_minimum_required(VERSION 3.10)

set(SOURCES
  main.cpp
  hash_traits.hpp
  coalesced_hashtable.hpp
)

add_executable(coalesced ${SOURCES})
