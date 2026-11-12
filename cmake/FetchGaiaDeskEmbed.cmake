# Download libgaiadesk_embed for this platform from a gaiadesk-embed-cpp
# release, verify it, unpack it and define GaiaDesk::embed.
#
# Self-contained: copy this one file into your project if you would rather
# not FetchContent the whole repository.
#
#   include(FetchGaiaDeskEmbed)
#   gaiadesk_embed_fetch()                       # the pinned version below
#   target_link_libraries(app PRIVATE GaiaDesk::embed)
#   gaiadesk_embed_deploy(app)
#
# Verification: the SHA-256 of each archive of the pinned version is in this
# file (written by scripts/package-release.sh), so a download that does not
# match is refused before anything is unpacked. For another version, pass
# SHA256 <hex>; failing that, the release's own .sha256 file is used and a
# warning says the check is only as good as that release.
#
# Options (cache variables):
#   GAIADESK_EMBED_BASE_URL   where the archives are (default: the GitHub
#                             release of the version; a file:// URL works)
#
# SPDX-License-Identifier: MIT
include_guard(GLOBAL)

set(GAIADESK_EMBED_PINNED_VERSION "0.3.1")
# BEGIN PINNED HASHES (scripts/package-release.sh rewrites this block)
set(GAIADESK_EMBED_SHA256_gaiadesk_embed-macos-universal.tar.gz "b859d3e099500af4cec5f065931e6ab6a82c24c2ca9c55aabffcf497b6ef77c5")
set(GAIADESK_EMBED_SHA256_gaiadesk_embed-linux-x64.tar.gz "b345a836893d3d889fac50781c8371d9318e82b7d4d0a550c4c03064d53b4aeb")
set(GAIADESK_EMBED_SHA256_gaiadesk_embed-win-x64.zip "25c157c78f67286fe4ca46abd108c5dc44b4e630e8d70c92203586927f5067dd")
# END PINNED HASHES

# The archive name for the platform being built for.
function(gaiadesk_embed_asset_name out)
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _cpu)
  if(APPLE)
    set(_asset "gaiadesk_embed-macos-universal.tar.gz")
  elseif(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8 OR _cpu MATCHES "arm")
      message(FATAL_ERROR "GaiaDeskEmbed: the Windows library is x64 only")
    endif()
    set(_asset "gaiadesk_embed-win-x64.zip")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND _cpu MATCHES "^(x86_64|amd64)$")
    set(_asset "gaiadesk_embed-linux-x64.tar.gz")
  else()
    message(FATAL_ERROR "GaiaDeskEmbed: no prebuilt library for ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
  set(${out} "${_asset}" PARENT_SCOPE)
endfunction()

# gaiadesk_embed_fetch([VERSION <v>] [SHA256 <hex>] [CXX_INCLUDE <dir>])
function(gaiadesk_embed_fetch)
  cmake_parse_arguments(GD "" "VERSION;SHA256;CXX_INCLUDE" "" ${ARGN})
  if(TARGET GaiaDesk::embed)
    return()
  endif()
  if(CMAKE_VERSION VERSION_LESS 3.18)
    message(FATAL_ERROR "gaiadesk_embed_fetch needs CMake 3.18 or later")
  endif()
  if(NOT GD_VERSION)
    set(GD_VERSION "${GAIADESK_EMBED_PINNED_VERSION}")
  endif()
  gaiadesk_embed_asset_name(_asset)
  set(GAIADESK_EMBED_BASE_URL "https://github.com/Gaia-Desk/gaiadesk-embed-cpp/releases/download/v${GD_VERSION}"
      CACHE STRING "Where the gaiadesk-embed-cpp release archives are")
  set(_url "${GAIADESK_EMBED_BASE_URL}/${_asset}")

  set(_sha "${GD_SHA256}")
  if(NOT _sha AND GD_VERSION STREQUAL GAIADESK_EMBED_PINNED_VERSION)
    set(_sha "${GAIADESK_EMBED_SHA256_${_asset}}")
  endif()
  set(_dir "${CMAKE_BINARY_DIR}/_gaiadesk_embed/${GD_VERSION}")
  file(MAKE_DIRECTORY "${_dir}")
  if(NOT _sha)
    message(WARNING "GaiaDeskEmbed ${GD_VERSION}: no pinned SHA-256 for ${_asset}; checking it against the release's own ${_asset}.sha256 (pass SHA256 to pin it)")
    file(DOWNLOAD "${_url}.sha256" "${_dir}/${_asset}.sha256" TLS_VERIFY ON STATUS _st)
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
      message(FATAL_ERROR "GaiaDeskEmbed: could not download ${_url}.sha256: ${_st}")
    endif()
    file(READ "${_dir}/${_asset}.sha256" _line)
    string(REGEX MATCH "^[0-9a-fA-F]+" _sha "${_line}")
  endif()
  string(TOLOWER "${_sha}" _sha)
  if(NOT _sha MATCHES "^[0-9a-f]+$" OR NOT _sha MATCHES "^................................................................$")
    message(FATAL_ERROR "GaiaDeskEmbed: '${_sha}' is not a SHA-256")
  endif()
  if(_sha MATCHES "^0+$")
    message(FATAL_ERROR "GaiaDeskEmbed: the pinned hash for ${_asset} is a placeholder; run scripts/package-release.sh")
  endif()

  set(_archive "${_dir}/${_asset}")
  set(_root "${_dir}/root")
  set(_stamp "${_root}/.gaiadesk-embed-sha256")
  set(_have "")
  if(EXISTS "${_stamp}")
    file(READ "${_stamp}" _have)
  endif()
  if(NOT _have STREQUAL _sha)
    # Skipped when the file is already there with this hash; refused (and
    # deleted) when the download does not match.
    message(STATUS "GaiaDeskEmbed: ${_url}")
    file(DOWNLOAD "${_url}" "${_archive}" EXPECTED_HASH SHA256=${_sha} TLS_VERIFY ON STATUS _st)
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
      file(REMOVE "${_archive}")
      message(FATAL_ERROR "GaiaDeskEmbed: could not download ${_url}: ${_st}")
    endif()
    file(REMOVE_RECURSE "${_root}")
    file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_root}")
    file(WRITE "${_stamp}" "${_sha}")
  endif()

  include("${_root}/lib/cmake/GaiaDeskEmbed/GaiaDeskEmbedImport.cmake")
  gaiadesk_embed_import(PREFIX "${_root}" CXX_INCLUDE "${GD_CXX_INCLUDE}")
  set(GAIADESK_EMBED_PREFIX "${_root}" PARENT_SCOPE)
  set(GAIADESK_EMBED_LIBRARY "${GAIADESK_EMBED_LIBRARY}" PARENT_SCOPE)
endfunction()
