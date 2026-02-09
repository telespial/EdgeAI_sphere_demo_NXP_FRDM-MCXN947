#!/usr/bin/env bash
set -euo pipefail

# User-local LinkServer install (no sudo).
#
# This downloads the official NXP LinkServer Linux x86_64 installer and extracts
# the LinkServer binary into ~/.local/opt, then symlinks ~/.local/bin/LinkServer.
#
# Running this script implies acceptance of the NXP LinkServer license terms.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VERSION="${LINKSERVER_VERSION:-25.12.83}"
BASE_URL="${LINKSERVER_BASE_URL:-https://www.nxp.com/lgfiles/updates/mcuxpresso}"

DL_DIR="${LINKSERVER_DL_DIR:-$ROOT_DIR/downloads}"
INSTALL_DIR="${LINKSERVER_INSTALL_DIR:-$HOME/.local/opt/linkserver/$VERSION}"
BIN_DIR="$HOME/.local/bin"

PKG="LinkServer_${VERSION}.x86_64.deb.bin"
URL="$BASE_URL/$PKG"

need_cmd() { command -v "$1" >/dev/null 2>&1; }

fetch_to() {
  local url="$1"
  local out="$2"
  if need_cmd curl; then
    curl -fL --retry 3 --retry-delay 2 -o "$out" "$url"
  elif need_cmd wget; then
    wget -O "$out" "$url"
  else
    echo "Need curl or wget to download LinkServer." >&2
    exit 1
  fi
}

if [[ "${ACCEPT_NXP_LINKSERVER_LICENSE:-0}" != "1" ]]; then
  echo "Set ACCEPT_NXP_LINKSERVER_LICENSE=1 to proceed (license acceptance)." >&2
  exit 1
fi

mkdir -p "$DL_DIR" "$INSTALL_DIR" "$BIN_DIR"

INSTALLER="$DL_DIR/$PKG"
echo "[linkserver] download: $URL"
if [[ ! -f "$INSTALLER" ]]; then
  fetch_to "$URL" "$INSTALLER"
fi

chmod +x "$INSTALLER"

TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "[linkserver] extracting installer -> $TMP_DIR"
# NXP's LinkServer installer may try to spawn a terminal (xterm-like) unless --nox11
# is provided; in minimal/headless setups this can fail with "Unknown option -title".
"$INSTALLER" --noexec --nox11 --noprogress --target "$TMP_DIR"

DEB="$(ls -1 "$TMP_DIR"/LinkServer_*.deb 2>/dev/null | head -n 1 || true)"
if [[ -z "$DEB" ]]; then
  echo "Could not locate LinkServer .deb after extraction in: $TMP_DIR" >&2
  echo "Contents:" >&2
  ls -la "$TMP_DIR" >&2 || true
  exit 1
fi

echo "[linkserver] unpacking deb -> $INSTALL_DIR"
rm -rf "$INSTALL_DIR"
mkdir -p "$INSTALL_DIR"
dpkg-deb -x "$DEB" "$INSTALL_DIR"

# Locate LinkServer binary inside the unpacked deb.
# Packaging has changed across versions (e.g. LinkServer_<ver>/dist/LinkServer).
LS_BIN=""
if [[ -x "$INSTALL_DIR/usr/local/LinkServer/LinkServer" ]]; then
  LS_BIN="$INSTALL_DIR/usr/local/LinkServer/LinkServer"
elif [[ -x "$INSTALL_DIR/usr/local/LinkServer_${VERSION}/dist/LinkServer" ]]; then
  LS_BIN="$INSTALL_DIR/usr/local/LinkServer_${VERSION}/dist/LinkServer"
elif [[ -x "$INSTALL_DIR/usr/local/LinkServer_${VERSION}/LinkServer" ]]; then
  LS_BIN="$INSTALL_DIR/usr/local/LinkServer_${VERSION}/LinkServer"
else
  LS_BIN="$(find "$INSTALL_DIR/usr/local" -type f -name LinkServer -perm -111 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "$LS_BIN" || ! -x "$LS_BIN" ]]; then
  echo "LinkServer binary not found after unpacking into: $INSTALL_DIR" >&2
  find "$INSTALL_DIR" -maxdepth 6 -type f -name LinkServer -print >&2 || true
  exit 1
fi

ln -sf "$LS_BIN" "$BIN_DIR/LinkServer"

echo "[linkserver] installed: $BIN_DIR/LinkServer -> $LS_BIN"
echo "[linkserver] version:"
"$BIN_DIR/LinkServer" --version 2>/dev/null || "$BIN_DIR/LinkServer" -h | head -n 2 || true
