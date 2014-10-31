project(audiodecoder.vgmstream)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(kodi REQUIRED)

include_directories(${KODI_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/lib/vgmstream)

add_subdirectory(lib/vgmstream)

set(VGM_SOURCES src/VGMCodec.cpp)

set(DEPLIBS vgmstream)

build_addon(audiodecoder.vgmstream VGM DEPLIBS)
