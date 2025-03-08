cmake_minimum_required(VERSION 3.10)


project(BassRGBWin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin/debug)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin/release)

set(CMAKE_CXX_STANDARD 17)

set(KISSFFT_DIR "${CMAKE_SOURCE_DIR}/src/kissfft")

include_directories(${KISSFFT_DIR})

add_executable(BassRGBWin src/BassRGBWin.cpp)

target_sources(BassRGBWin PRIVATE
    ${KISSFFT_DIR}/kiss_fft.c
    ${KISSFFT_DIR}/kiss_fftr.c
)
