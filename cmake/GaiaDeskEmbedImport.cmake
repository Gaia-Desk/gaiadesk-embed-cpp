# GaiaDesk::embed from an unpacked binary archive, and gaiadesk_embed_deploy().
#
# Shipped in every release archive (lib/cmake/GaiaDeskEmbed/), where
# GaiaDeskEmbedConfig.cmake calls it with the archive's root. The layout:
#
#   include/gaiadesk_embed.h          the C API
#   include/gaiadesk/embed.hpp        the C++17 wrapper (header-only)
#   lib/libgaiadesk_embed.dylib       macOS (universal; install name @rpath/…)
#   lib/libgaiadesk_embed.so          Linux x86_64 (no SONAME)
#   bin/gaiadesk_embed.dll            Windows x64
#   lib/gaiadesk_embed.dll.lib        Windows x64 import library
#
# SPDX-License-Identifier: MIT
include_guard(GLOBAL)

# gaiadesk_embed_import(PREFIX <dir> [CXX_INCLUDE <dir>])
#   Define the imported target GaiaDesk::embed (C and C++; C++ needs C++17)
#   from the binary layout under <dir>. CXX_INCLUDE puts another directory
#   of the C++ headers in front (a source checkout of this repository).
function(gaiadesk_embed_import)
  cmake_parse_arguments(GD "" "PREFIX;CXX_INCLUDE" "" ${ARGN})
  if(TARGET GaiaDesk::embed)
    return()
  endif()
  if(NOT GD_PREFIX OR NOT EXISTS "${GD_PREFIX}/include/gaiadesk_embed.h")
    message(FATAL_ERROR "GaiaDeskEmbed: no include/gaiadesk_embed.h under '${GD_PREFIX}'")
  endif()

  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _gd_cpu)
  if(APPLE)
    set(_gd_lib "${GD_PREFIX}/lib/libgaiadesk_embed.dylib")
  elseif(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR _gd_cpu MATCHES "arm")
      message(FATAL_ERROR "GaiaDeskEmbed: the Windows library is x64 only (this build: ${CMAKE_SYSTEM_PROCESSOR}, ${CMAKE_SIZEOF_VOID_P}-byte pointers)")
    endif()
    set(_gd_lib "${GD_PREFIX}/bin/gaiadesk_embed.dll")
    set(_gd_implib "${GD_PREFIX}/lib/gaiadesk_embed.dll.lib")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT _gd_cpu MATCHES "^(x86_64|amd64)$")
      message(FATAL_ERROR "GaiaDeskEmbed: the Linux library is x86_64 only (this build: ${CMAKE_SYSTEM_PROCESSOR})")
    endif()
    set(_gd_lib "${GD_PREFIX}/lib/libgaiadesk_embed.so")
  else()
    message(FATAL_ERROR "GaiaDeskEmbed: no prebuilt library for ${CMAKE_SYSTEM_NAME}")
  endif()
  if(NOT EXISTS "${_gd_lib}")
    message(FATAL_ERROR "GaiaDeskEmbed: ${_gd_lib} is missing (is this the archive for ${CMAKE_SYSTEM_NAME}?)")
  endif()

  add_library(GaiaDesk::embed SHARED IMPORTED GLOBAL)
  set_target_properties(GaiaDesk::embed PROPERTIES IMPORTED_LOCATION "${_gd_lib}")
  if(APPLE)
    set_target_properties(GaiaDesk::embed PROPERTIES
      IMPORTED_SONAME "@rpath/libgaiadesk_embed.dylib"
      # embed_macos.hpp talks to the Objective-C runtime.
      INTERFACE_LINK_LIBRARIES "-lobjc")
  elseif(WIN32)
    if(NOT EXISTS "${_gd_implib}")
      message(FATAL_ERROR "GaiaDeskEmbed: ${_gd_implib} is missing")
    endif()
    set_target_properties(GaiaDesk::embed PROPERTIES IMPORTED_IMPLIB "${_gd_implib}")
  else()
    # The .so has no SONAME: link it by name, so executables record
    # "libgaiadesk_embed.so" (found through their RPATH) and not this path.
    set_target_properties(GaiaDesk::embed PROPERTIES IMPORTED_NO_SONAME TRUE)
  endif()

  set(_gd_inc "${GD_PREFIX}/include")
  if(GD_CXX_INCLUDE)
    set(_gd_inc "${GD_CXX_INCLUDE};${_gd_inc}")
  endif()
  set_target_properties(GaiaDesk::embed PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_gd_inc}")
  set(GAIADESK_EMBED_LIBRARY "${_gd_lib}" PARENT_SCOPE)
  set(GAIADESK_EMBED_PREFIX "${GD_PREFIX}" PARENT_SCOPE)
endfunction()

# gaiadesk_embed_deploy(<target>)
#   Copy the shared library next to <target>'s executable (into
#   Contents/Frameworks of a macOS app bundle) after every build, and point
#   the executable's RPATH there ($ORIGIN, @loader_path,
#   @executable_path/../Frameworks). On Windows the DLL simply sits beside
#   the .exe. Install rules stay yours: install the same file to the same
#   place relative to your installed executable.
function(gaiadesk_embed_deploy target)
  if(NOT TARGET GaiaDesk::embed)
    message(FATAL_ERROR "gaiadesk_embed_deploy: GaiaDesk::embed is not defined (find_package(GaiaDeskEmbed) first)")
  endif()
  get_target_property(_gd_bundle ${target} MACOSX_BUNDLE)
  if(APPLE AND _gd_bundle)
    set(_gd_dest "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks")
    set(_gd_rpath "@executable_path/../Frameworks")
  else()
    set(_gd_dest "$<TARGET_FILE_DIR:${target}>")
    if(APPLE)
      set(_gd_rpath "@loader_path")
    elseif(NOT WIN32)
      set(_gd_rpath "$ORIGIN")
    endif()
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_gd_dest}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:GaiaDesk::embed>" "${_gd_dest}"
    VERBATIM)
  if(_gd_rpath)
    set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "${_gd_rpath}")
    set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH "${_gd_rpath}")
  endif()
endfunction()
