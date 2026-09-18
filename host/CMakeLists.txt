cmake_minimum_required(VERSION 3.16)
project(needle_host C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

set(ENGINE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../engine)

file(GLOB ENGINE_SRC ${ENGINE_DIR}/src/*.c)

# ND_PROFILE=ON instruments the forward pass with the same phase counters the
# firmware prints, so a host breakdown is comparable to the device one. It adds
# a clock() per phase: fine for attribution, never for a speed number.
option(ND_PROFILE "Instrument the forward pass per phase" OFF)

add_library(needle_engine STATIC ${ENGINE_SRC})
target_include_directories(needle_engine PUBLIC ${ENGINE_DIR}/include)
target_link_libraries(needle_engine PUBLIC m)
if(ND_PROFILE)
  target_compile_definitions(needle_engine PUBLIC ND_PROFILE)
endif()

add_executable(nd_dump nd_dump.c)
target_link_libraries(nd_dump PRIVATE needle_engine)

add_executable(nd_gtest nd_gtest.c)
target_link_libraries(nd_gtest PRIVATE needle_engine)

enable_testing()
add_test(NAME grammar COMMAND nd_gtest)

add_executable(nd_mlpstat nd_mlpstat.c)
target_link_libraries(nd_mlpstat PRIVATE needle_engine)


add_executable(nd_ftest nd_ftest.c)
target_link_libraries(nd_ftest PRIVATE needle_engine)

add_executable(nd_ptest nd_ptest.c)
target_link_libraries(nd_ptest PRIVATE needle_engine)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../model/needle3.cact")
  add_test(NAME prefix_isolation COMMAND nd_ptest "${CMAKE_CURRENT_SOURCE_DIR}/../model/needle3.cact")
endif()
