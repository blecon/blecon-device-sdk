# Copyright (c) Blecon Ltd
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)

project(blecon)

include(CMakeDependentOption)
cmake_dependent_option(BLECON_ZEPHYR_PORT "Compile Zephyr port" ON "DEFINED ZEPHYR_BASE" OFF)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${PROJECT_SOURCE_DIR}/third-party/nanopb/extra)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

enable_language(ASM C)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/blecon-source.cmake")
  set(BLECON_HAS_SOURCE TRUE)
else()
  set(BLECON_HAS_SOURCE FALSE)
endif()

cmake_dependent_option(BLECON_BUILD_FROM_SOURCE "Compile from source" ON "BLECON_HAS_SOURCE" OFF)

if((NOT (DEFINED ZEPHYR_BASE)) OR CONFIG_BLECON)
  if(BLECON_BUILD_FROM_SOURCE)
      include(cmake/blecon-source.cmake)
  else()
      include(cmake/blecon-prebuilt.cmake)
  endif()

  if(BLECON_ZEPHYR_PORT)
    add_subdirectory("ports/zephyr")
  endif()
endif()