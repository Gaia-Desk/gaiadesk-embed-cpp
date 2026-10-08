# gaiadesk-embed-cpp: libgaiadesk_embed for C and C++.
#
# As a dependency (FetchContent / add_subdirectory) this defines
# GaiaDesk::embed and gaiadesk_embed_deploy(<target>), downloading the
# prebuilt library for the platform from this repository's release (SHA-256
# pinned in cmake/FetchGaiaDeskEmbed.cmake), or using GAIADESK_EMBED_ROOT.
#
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.18)
project(GaiaDeskEmbed VERSION 0.1.0 DESCRIPTION "GaiaDesk embed SDK for C and C++" LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(_gd_top ON)
else()
  set(_gd_top OFF)
endif()

set(GAIADESK_EMBED_ROOT "" CACHE PATH
    "An unpacked release archive (or installed prefix) to use instead of downloading one")
option(GAIADESK_EMBED_BUILD_TESTS "Build the tests" ${_gd_top})
option(GAIADESK_EMBED_BUILD_EXAMPLES "Build the examples" ${_gd_top})

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GaiaDeskEmbedImport.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchGaiaDeskEmbed.cmake")

# The C++ headers come from this checkout (they are also in every archive).
if(GAIADESK_EMBED_ROOT)
  gaiadesk_embed_import(PREFIX "${GAIADESK_EMBED_ROOT}" CXX_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")
else()
  gaiadesk_embed_fetch(VERSION "${PROJECT_VERSION}" CXX_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()
message(STATUS "GaiaDeskEmbed: ${GAIADESK_EMBED_PREFIX}")

if(GAIADESK_EMBED_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
if(GAIADESK_EMBED_BUILD_EXAMPLES)
  add_subdirectory(examples/c-minimal)
  find_package(Qt6 COMPONENTS Widgets QUIET)
  if(Qt6_FOUND)
    add_subdirectory(examples/qt-helpdesk)
  else()
    message(STATUS "GaiaDeskEmbed: Qt 6 Widgets not found; examples/qt-helpdesk is not built")
  endif()
endif()

# cmake --install: the release archive's layout, usable with
# find_package(GaiaDeskEmbed) from CMAKE_INSTALL_PREFIX.
if(_gd_top)
  install(DIRECTORY include/gaiadesk DESTINATION include)
  install(FILES "${GAIADESK_EMBED_PREFIX}/include/gaiadesk_embed.h" DESTINATION include)
  if(WIN32)
    install(FILES "${GAIADESK_EMBED_PREFIX}/bin/gaiadesk_embed.dll" DESTINATION bin)
    install(FILES "${GAIADESK_EMBED_PREFIX}/lib/gaiadesk_embed.dll.lib" DESTINATION lib)
  else()
    get_target_property(_gd_lib GaiaDesk::embed IMPORTED_LOCATION)
    install(PROGRAMS "${_gd_lib}" DESTINATION lib)
  endif()
  set(GAIADESK_EMBED_VERSION "${PROJECT_VERSION}")
  configure_file(cmake/GaiaDeskEmbedConfigVersion.cmake.in
                 "${CMAKE_CURRENT_BINARY_DIR}/GaiaDeskEmbedConfigVersion.cmake" @ONLY)
  install(FILES cmake/GaiaDeskEmbedConfig.cmake cmake/GaiaDeskEmbedImport.cmake
                "${CMAKE_CURRENT_BINARY_DIR}/GaiaDeskEmbedConfigVersion.cmake"
          DESTINATION lib/cmake/GaiaDeskEmbed)
  install(FILES LICENSE LICENSE-BINARY DESTINATION share/doc/gaiadesk-embed)
endif()
