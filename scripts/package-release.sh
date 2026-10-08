#!/usr/bin/env bash
# Assemble this repository's release archives from libgaiadesk_embed's build
# output, with a .sha256 beside each, into dist/; then pin their hashes in
# cmake/FetchGaiaDeskEmbed.cmake (commit that change with the release).
#
#   scripts/package-release.sh <embed-out-dir> [--no-pin]
#
# <embed-out-dir> holds the library builds (GaiaDesk's embed/out):
#   mac/libgaiadesk_embed.dylib, mac/headers/gaiadesk_embed.h
#   windows/gaiadesk_embed-win-x64.zip  (gaiadesk_embed.dll, .dll.lib, header)
#   linux/libgaiadesk_embed.so, linux/gaiadesk_embed.h
#
# Writes dist/:
#   gaiadesk_embed-macos-universal.tar.gz  (+ .sha256)
#   gaiadesk_embed-linux-x64.tar.gz        (+ .sha256)
#   gaiadesk_embed-win-x64.zip             (+ .sha256)
#   SHA256SUMS
#
# Every archive has the same layout (see cmake/GaiaDeskEmbedImport.cmake): the
# C header, this repo's C++ headers, the library, lib/cmake/GaiaDeskEmbed/
# (find_package), LICENSE (MIT, the C++ and CMake files) and LICENSE-BINARY
# (the library). Archives are reproducible: fixed timestamps, owners and
# order, so the same inputs give the same hashes.
#
# Refuses: headers that differ between platforms, a licence that is not this
# repo's LICENSE-BINARY, a test-source build (NOT-FOR-RELEASE marker or the
# test-pattern source in the binary), a binary of the wrong OS/CPU, a macOS
# library that links libvpx or lacks a slice, or one whose version is not the
# project's.
set -euo pipefail

usage() { echo "usage: scripts/package-release.sh <embed-out-dir> [--no-pin]" >&2; exit 2; }
[ $# -ge 1 ] || usage
OUT="$(cd "$1" && pwd)"
PIN=1
[ "${2:-}" = "--no-pin" ] && PIN=0

REPO="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(sed -n 's/^project(GaiaDeskEmbed VERSION \([0-9.]*\).*/\1/p' "$REPO/CMakeLists.txt")"
[ -n "$VERSION" ] || { echo "no version in CMakeLists.txt" >&2; exit 1; }
DIST="$REPO/dist"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

die() { echo "package-release: $*" >&2; exit 1; }
sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }
lf() { tr -d '\r' < "$1"; }

# ---- inputs ----------------------------------------------------------------
MAC_LIB="$OUT/mac/libgaiadesk_embed.dylib"
MAC_H="$OUT/mac/headers/gaiadesk_embed.h"
LINUX_LIB="$OUT/linux/libgaiadesk_embed.so"
LINUX_H="$OUT/linux/gaiadesk_embed.h"
WIN_ZIP="$OUT/windows/gaiadesk_embed-win-x64.zip"
for f in "$MAC_LIB" "$MAC_H" "$LINUX_LIB" "$LINUX_H" "$WIN_ZIP"; do
  [ -f "$f" ] || die "missing $f"
done
for d in "$OUT/mac" "$OUT/mac/GaiaDeskEmbedFFI.xcframework" "$OUT/linux" "$OUT/windows"; do
  [ ! -e "$d/NOT-FOR-RELEASE" ] || die "$d is a test-source build (NOT-FOR-RELEASE)"
done
mkdir -p "$WORK/win"
unzip -q "$WIN_ZIP" -d "$WORK/win"
WIN_DLL="$WORK/win/gaiadesk_embed.dll"
WIN_LIB="$WORK/win/gaiadesk_embed.dll.lib"
WIN_H="$WORK/win/gaiadesk_embed.h"
for f in "$WIN_DLL" "$WIN_LIB" "$WIN_H"; do [ -f "$f" ] || die "$(basename "$f") is not in $WIN_ZIP"; done
[ ! -e "$WORK/win/NOT-FOR-RELEASE" ] || die "$WIN_ZIP is a test-source build"

# ---- checks ----------------------------------------------------------------
lf "$MAC_H" > "$WORK/gaiadesk_embed.h"
for h in "$LINUX_H" "$WIN_H"; do
  cmp -s "$WORK/gaiadesk_embed.h" <(lf "$h") || die "$h differs from $MAC_H (one platform is from another build)"
done
for l in "$OUT/linux/LICENSE-BINARY" "$WORK/win/LICENSE-BINARY"; do
  [ ! -f "$l" ] || cmp -s <(lf "$REPO/LICENSE-BINARY") <(lf "$l") || die "$l differs from this repo's LICENSE-BINARY"
done
for b in "$MAC_LIB" "$LINUX_LIB" "$WIN_DLL"; do
  if LC_ALL=C grep -q -a "test-pattern" "$b"; then die "$b is a test-source build (has the test-pattern source)"; fi
done
file -b "$LINUX_LIB" | grep -q "ELF 64-bit LSB shared object, x86-64" || die "$LINUX_LIB is not an x86-64 ELF shared object: $(file -b "$LINUX_LIB")"
file -b "$WIN_DLL" | grep -q "PE32+ executable (DLL).*x86-64" || die "$WIN_DLL is not an x64 DLL: $(file -b "$WIN_DLL")"
file -b "$MAC_LIB" | grep -q "Mach-O universal binary" || die "$MAC_LIB is not a universal Mach-O: $(file -b "$MAC_LIB")"
if command -v lipo >/dev/null; then
  archs="$(lipo -archs "$MAC_LIB")"
  for a in arm64 x86_64; do
    case " $archs " in *" $a "*) ;; *) die "$MAC_LIB has no $a slice ($archs)";; esac
  done
fi
if command -v otool >/dev/null; then
  ! otool -L "$MAC_LIB" | grep -qi vpx || die "$MAC_LIB links libvpx"
  [ "$(otool -D "$MAC_LIB" | tail -1)" = "@rpath/libgaiadesk_embed.dylib" ] || die "$MAC_LIB install name is not @rpath/libgaiadesk_embed.dylib"
fi
# The macOS library can be asked what it is (here, on a Mac).
if [ "$(uname -s)" = "Darwin" ]; then
  info="$(python3 -I -c 'import ctypes,sys
lib = ctypes.CDLL(sys.argv[1])
lib.gd_embed_build_info.restype = ctypes.c_char_p
print(lib.gd_embed_build_info().decode())' "$MAC_LIB")"
  echo "macOS build_info: $info"
  python3 -I -c 'import json,sys
i = json.loads(sys.argv[1])
sys.exit(0 if i.get("test_source") is False and i.get("version") == sys.argv[2] else 1)' "$info" "$VERSION" \
    || die "the macOS library is not a release build of $VERSION: $info"
fi

# ---- staging ---------------------------------------------------------------
stage_common() {
  local s="$1"
  mkdir -p "$s/include/gaiadesk/detail" "$s/lib/cmake/GaiaDeskEmbed"
  cp "$WORK/gaiadesk_embed.h" "$s/include/gaiadesk_embed.h"
  cp "$REPO/include/gaiadesk/"*.hpp "$s/include/gaiadesk/"
  cp "$REPO/include/gaiadesk/detail/"*.hpp "$s/include/gaiadesk/detail/"
  cp "$REPO/cmake/GaiaDeskEmbedConfig.cmake" "$REPO/cmake/GaiaDeskEmbedImport.cmake" "$s/lib/cmake/GaiaDeskEmbed/"
  sed "s/@GAIADESK_EMBED_VERSION@/$VERSION/g" "$REPO/cmake/GaiaDeskEmbedConfigVersion.cmake.in" \
    > "$s/lib/cmake/GaiaDeskEmbed/GaiaDeskEmbedConfigVersion.cmake"
  cp "$REPO/LICENSE" "$REPO/LICENSE-BINARY" "$s/"
}

S_MAC="$WORK/stage/macos"; stage_common "$S_MAC"; cp "$MAC_LIB" "$S_MAC/lib/"
S_LINUX="$WORK/stage/linux"; stage_common "$S_LINUX"; cp "$LINUX_LIB" "$S_LINUX/lib/"
S_WIN="$WORK/stage/win"; stage_common "$S_WIN"; mkdir -p "$S_WIN/bin"
cp "$WIN_DLL" "$S_WIN/bin/"; cp "$WIN_LIB" "$S_WIN/lib/"

normalize() {
  local s="$1"
  find "$s" -type d -exec chmod 755 {} +
  find "$s" -type f -exec chmod 644 {} +
  find "$s" -type f \( -name '*.dylib' -o -name '*.so' -o -name '*.dll' \) -exec chmod 755 {} +
  find "$s" -exec env TZ=UTC touch -h -t 202601010000 {} +
}

tgz() {  # <stage> <out.tar.gz>
  local s="$1" out="$2"
  normalize "$s"
  (cd "$s" && find . -type f | LC_ALL=C sort | sed 's|^\./||' > "$WORK/list")
  if tar --version 2>/dev/null | grep -q "GNU tar"; then
    tar -C "$s" --owner=0 --group=0 --numeric-owner --mtime='2026-01-01 00:00Z' --format=ustar \
      -cf "$WORK/a.tar" -T "$WORK/list"
  else
    tar -C "$s" --uid 0 --gid 0 --uname root --gname root --format ustar -cf "$WORK/a.tar" -T "$WORK/list"
  fi
  gzip -n -9 -c "$WORK/a.tar" > "$out"
}

zipit() {  # <stage> <out.zip>
  local s="$1" out="$2"
  normalize "$s"
  rm -f "$out"
  (cd "$s" && find . -type f | LC_ALL=C sort | sed 's|^\./||' | TZ=UTC zip -q -X -D -9 "$out" -@)
}

rm -rf "$DIST"
mkdir -p "$DIST"
tgz "$S_MAC" "$DIST/gaiadesk_embed-macos-universal.tar.gz"
tgz "$S_LINUX" "$DIST/gaiadesk_embed-linux-x64.tar.gz"
zipit "$S_WIN" "$DIST/gaiadesk_embed-win-x64.zip"

: > "$DIST/SHA256SUMS"
for a in gaiadesk_embed-macos-universal.tar.gz gaiadesk_embed-linux-x64.tar.gz gaiadesk_embed-win-x64.zip; do
  h="$(sha256 "$DIST/$a")"
  printf '%s  %s\n' "$h" "$a" > "$DIST/$a.sha256"
  printf '%s  %s\n' "$h" "$a" >> "$DIST/SHA256SUMS"
  if [ "$PIN" = 1 ]; then
    sed -i.bak "s|^set(GAIADESK_EMBED_SHA256_$a \"[0-9a-f]*\")|set(GAIADESK_EMBED_SHA256_$a \"$h\")|" "$REPO/cmake/FetchGaiaDeskEmbed.cmake"
    rm -f "$REPO/cmake/FetchGaiaDeskEmbed.cmake.bak"
    grep -q "GAIADESK_EMBED_SHA256_$a \"$h\"" "$REPO/cmake/FetchGaiaDeskEmbed.cmake" || die "could not pin $a"
  fi
done
if [ "$PIN" = 1 ]; then
  sed -i.bak "s|^set(GAIADESK_EMBED_PINNED_VERSION \"[^\"]*\")|set(GAIADESK_EMBED_PINNED_VERSION \"$VERSION\")|" "$REPO/cmake/FetchGaiaDeskEmbed.cmake"
  rm -f "$REPO/cmake/FetchGaiaDeskEmbed.cmake.bak"
fi

echo "gaiadesk-embed-cpp $VERSION -> $DIST"
(cd "$DIST" && ls -l && cat SHA256SUMS)
