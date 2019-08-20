cmake_minimum_required(VERSION 3.5)
project(audiodecoder.vgmstream)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)

include_directories(${KODI_INCLUDE_DIR}/..
                    ${PROJECT_SOURCE_DIR}/lib/vgmstream)

add_subdirectory(lib/vgmstream)

set(VGM_SOURCES src/VGMCodec.cpp)

set(DEPLIBS vgmstream)

build_addon(audiodecoder.vgmstream VGM DEPLIBS)

include(CPack)
