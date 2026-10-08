# find_package(GaiaDeskEmbed) for an unpacked release archive or an
# installed copy: lib/cmake/GaiaDeskEmbed/ under the prefix. Defines
# GaiaDesk::embed and gaiadesk_embed_deploy(<target>).
#
# SPDX-License-Identifier: MIT
include("${CMAKE_CURRENT_LIST_DIR}/GaiaDeskEmbedImport.cmake")
get_filename_component(_gd_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
gaiadesk_embed_import(PREFIX "${_gd_prefix}")
set(GaiaDeskEmbed_FOUND TRUE)
