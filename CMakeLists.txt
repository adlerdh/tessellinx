cmake_minimum_required(VERSION 3.16)

project(tessellinx VERSION 1.0.0 LANGUAGES CXX)

include(FetchContent)
include(ExternalProject)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(TESSELLINX_BUILD_DEPS "Build x264/ffmpeg from source (Unix-like default)" ON)

# x264 pinned to a specific commit SHA
set(X264_GIT_REV "b35605ace3ddf7c1a5d67a2eb553f034aef41d55")
set(FFMPEG_VER "8.0.1")
set(FFMPEG_SHA256 "05ee0b03119b45c0bdb4df654b96802e909e0a752f72e4fe3794f487229e5a41")

# Local install prefixes for ExternalProject builds
set(X264_INSTALL_DIR   "${CMAKE_BINARY_DIR}/_deps/install/x264")
set(FFMPEG_INSTALL_DIR "${CMAKE_BINARY_DIR}/_deps/install/ffmpeg")

# --- CLI11 ---
FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.5.0
)
FetchContent_MakeAvailable(CLI11)

# --- Dependency build strategy ---
set(TESSELLINX_HAVE_LOCAL_FFMPEG FALSE)

if(WIN32 AND TESSELLINX_BUILD_DEPS)
  message(WARNING
    "TESSELLINX_BUILD_DEPS=ON on Windows: building x264/ffmpeg from source usually requires MSYS2/MinGW.\n"
    "If you are using MSVC without a Unix-like environment, set TESSELLINX_BUILD_DEPS=OFF and use a prebuilt/system FFmpeg."
  )
endif()

if(TESSELLINX_BUILD_DEPS AND NOT WIN32)
  set(TESSELLINX_HAVE_LOCAL_FFMPEG TRUE)

  # CMake validates INTERFACE_INCLUDE_DIRECTORIES at *generate* time.
  # ExternalProject installs happen at *build* time, so create the expected dirs now.
  file(MAKE_DIRECTORY "${X264_INSTALL_DIR}/include")
  file(MAKE_DIRECTORY "${X264_INSTALL_DIR}/lib")
  file(MAKE_DIRECTORY "${FFMPEG_INSTALL_DIR}/include")
  file(MAKE_DIRECTORY "${FFMPEG_INSTALL_DIR}/lib")

  # Parallelism: honor CMAKE_BUILD_PARALLEL_LEVEL when set, else fallback.
  if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(_PARALLEL "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
  else()
    set(_PARALLEL "4")
  endif()

  # --- x264 ---
  ExternalProject_Add(
    x264_ep
    GIT_REPOSITORY https://code.videolan.org/videolan/x264.git
    GIT_TAG        ${X264_GIT_REV}
    GIT_SHALLOW    TRUE
    UPDATE_DISCONNECTED TRUE

    PREFIX "${CMAKE_BINARY_DIR}/_deps/build/x264"
    BUILD_IN_SOURCE 1

    CONFIGURE_COMMAND
      ./configure
        --prefix=${X264_INSTALL_DIR}
        --enable-shared
        --disable-cli

    BUILD_COMMAND   ${CMAKE_COMMAND} -E env MAKEFLAGS=-j${_PARALLEL} make
    INSTALL_COMMAND make install
  )

  # --- FFmpeg (depends on x264) ---
  ExternalProject_Add(
    ffmpeg_ep
    DEPENDS x264_ep
    URL "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VER}.tar.xz"
    URL_HASH "SHA256=${FFMPEG_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP true

    PREFIX "${CMAKE_BINARY_DIR}/_deps/build/ffmpeg"
    BUILD_IN_SOURCE 0

    CONFIGURE_COMMAND
      ${CMAKE_COMMAND} -E env
        PKG_CONFIG_PATH=${X264_INSTALL_DIR}/lib/pkgconfig
      <SOURCE_DIR>/configure
        --prefix=${FFMPEG_INSTALL_DIR}
        --disable-doc
        --disable-programs
        --disable-debug
        --disable-static
        --enable-shared
        --enable-pic
        --enable-gpl
        --enable-libx264
        --extra-cflags=-I${X264_INSTALL_DIR}/include
        --extra-ldflags=-L${X264_INSTALL_DIR}/lib

    BUILD_COMMAND   ${CMAKE_COMMAND} -E env MAKEFLAGS=-j${_PARALLEL} make
    INSTALL_COMMAND make install
  )
endif()

# --- Imported targets for FFmpeg ---
# When we build FFmpeg locally, create imported SHARED libs pointing to the local install.
function(tessellinx_add_imported_ffmpeg_shared target_name lib_basename)
  add_library(${target_name} SHARED IMPORTED GLOBAL)
  set_target_properties(${target_name} PROPERTIES
    IMPORTED_LOCATION "${FFMPEG_INSTALL_DIR}/lib/lib${lib_basename}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INSTALL_DIR}/include"
  )
endfunction()

if(TESSELLINX_HAVE_LOCAL_FFMPEG)
  tessellinx_add_imported_ffmpeg_shared(FFmpeg::avcodec  avcodec)
  tessellinx_add_imported_ffmpeg_shared(FFmpeg::avformat avformat)
  tessellinx_add_imported_ffmpeg_shared(FFmpeg::avutil   avutil)
  tessellinx_add_imported_ffmpeg_shared(FFmpeg::swscale  swscale)
else()
  # System / prebuilt FFmpeg (recommended for Windows)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(PC_AVCODEC  REQUIRED IMPORTED_TARGET libavcodec)
  pkg_check_modules(PC_AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
  pkg_check_modules(PC_AVUTIL   REQUIRED IMPORTED_TARGET libavutil)
  pkg_check_modules(PC_SWSCALE  REQUIRED IMPORTED_TARGET libswscale)

  add_library(FFmpeg::avcodec  ALIAS PkgConfig::PC_AVCODEC)
  add_library(FFmpeg::avformat ALIAS PkgConfig::PC_AVFORMAT)
  add_library(FFmpeg::avutil   ALIAS PkgConfig::PC_AVUTIL)
  add_library(FFmpeg::swscale  ALIAS PkgConfig::PC_SWSCALE)
endif()

# --- Your targets ---
add_library(video_encoder SHARED video.cxx)

if(TESSELLINX_HAVE_LOCAL_FFMPEG)
  add_dependencies(video_encoder ffmpeg_ep)
endif()

target_link_libraries(video_encoder
  PUBLIC
    FFmpeg::avcodec
    FFmpeg::avformat
    FFmpeg::avutil
    FFmpeg::swscale
)

# RPATH for locally-built FFmpeg on Unix-like systems
if(TESSELLINX_HAVE_LOCAL_FFMPEG AND UNIX AND NOT APPLE)
  set_target_properties(video_encoder PROPERTIES
    BUILD_RPATH   "${FFMPEG_INSTALL_DIR}/lib"
    INSTALL_RPATH "${FFMPEG_INSTALL_DIR}/lib"
  )
elseif(TESSELLINX_HAVE_LOCAL_FFMPEG AND APPLE)
  set_target_properties(video_encoder PROPERTIES
    BUILD_RPATH   "${FFMPEG_INSTALL_DIR}/lib"
    INSTALL_RPATH "@loader_path"
  )
endif()

add_executable(tessellinx
  main.cxx
  colors.cxx
  dlx.cxx
  shapes.cxx
  reporting.cxx
)

target_link_libraries(tessellinx PRIVATE video_encoder CLI11::CLI11)
