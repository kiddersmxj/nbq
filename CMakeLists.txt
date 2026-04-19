cmake_minimum_required(VERSION 3.11)
 
# --- Fetch libs --------------------------------------------------------------
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.11)

project(nbq
  LANGUAGES CXX
  VERSION 1.0.0
)

add_executable(nbq src/nbq.cpp)

find_package(std-k REQUIRED)

target_include_directories(nbq
    PRIVATE inc
)

install(
    TARGETS nbq
    DESTINATION /usr/local/bin/
)

target_link_libraries(nbq
    PRIVATE std-k
)

target_compile_features(nbq PUBLIC cxx_std_20)

target_sources(nbq
    PRIVATE src/nbq.cpp
    PRIVATE src/config.cpp
    PRIVATE src/parser.cpp
    PRIVATE src/model.cpp
    PRIVATE src/commands.cpp
    PRIVATE src/mcu.cpp
)


# Copyright (c) 2026, Maxamilian Kidd-May
# All rights reserved.

# This source code is licensed under the MIT license found in the
# LICENSE file in the root directory of this source tree.

