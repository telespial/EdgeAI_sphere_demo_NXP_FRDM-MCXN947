#!/usr/bin/env bash
set -euo pipefail

# User-local bootstrap for fresh Ubuntu installs without sudo.
#
# Installs:
# - pip (user-local)
# - west, cmake, ninja (user-local via pip)
# - git-lfs (user-local)
# - arm-none-eabi-gcc toolchain (user-local, xPack)

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "$HOME/.local/bin" "$HOME/.local/opt" "$HOME/.cache/edgeai_sand_demo"
export PATH="$HOME/.local/bin:$PATH"

need_cmd() { command -v "$1" >/dev/null 2>&1; }

fetch_to() {
  local url="$1"
  local out="$2"
  if need_cmd curl; then
    curl -fsSL "$url" -o "$out"
  elif need_cmd wget; then
    wget -qO "$out" "$url"
  else
    echo "[bootstrap] missing downloader (need curl or wget)" >&2
    exit 1
  fi
}

echo "[bootstrap] repo: $ROOT_DIR"

if ! need_cmd python3; then
  echo "[bootstrap] python3 missing; install python3 via OS package manager." >&2
  exit 1
fi

if ! python3 -m pip -V >/dev/null 2>&1; then
  echo "[bootstrap] installing pip (user-local)"
  GETPIP="$HOME/.cache/edgeai_sand_demo/get-pip.py"
  fetch_to https://bootstrap.pypa.io/get-pip.py "$GETPIP"
  python3 "$GETPIP" --user --break-system-packages
fi

echo "[bootstrap] upgrading pip/setuptools/wheel (user-local)"
python3 -m pip install --user --break-system-packages --upgrade pip setuptools wheel

echo "[bootstrap] installing west/cmake/ninja (user-local)"
python3 -m pip install --user --break-system-packages --upgrade west cmake ninja jsonschema

if ! need_cmd git-lfs; then
  echo "[bootstrap] installing git-lfs (user-local)"
  python3 - <<'PY'
import json
import os
import re
import sys
import tarfile
import urllib.request

home = os.path.expanduser("~")
cache = os.path.join(home, ".cache", "edgeai_sand_demo")
os.makedirs(cache, exist_ok=True)

api = "https://api.github.com/repos/git-lfs/git-lfs/releases/latest"
with urllib.request.urlopen(api) as r:
    data = json.load(r)

assets = data.get("assets", [])
url = None
name = None
for a in assets:
    n = a.get("name", "")
    if re.search(r"git-lfs-linux-amd64-.*\.tar\.gz$", n):
        url = a.get("browser_download_url")
        name = n
        break

if not url:
    print("Could not find git-lfs linux-amd64 asset in latest release.", file=sys.stderr)
    sys.exit(1)

tar_path = os.path.join(cache, name)
urllib.request.urlretrieve(url, tar_path)

with tarfile.open(tar_path, "r:gz") as tf:
    bin_member = None
    for m in tf.getmembers():
        if m.isfile() and m.name.endswith("/git-lfs"):
            bin_member = m
            break
    if not bin_member:
        print("Could not locate git-lfs binary in archive.", file=sys.stderr)
        sys.exit(1)

    f = tf.extractfile(bin_member)
    assert f is not None
    out_path = os.path.join(home, ".local", "bin", "git-lfs")
    with open(out_path, "wb") as out:
        out.write(f.read())
    os.chmod(out_path, 0o755)

print("Installed git-lfs to ~/.local/bin/git-lfs")
PY
fi

if need_cmd git && need_cmd git-lfs; then
  git lfs install --skip-repo >/dev/null 2>&1 || true
fi

if ! need_cmd arm-none-eabi-gcc; then
  echo "[bootstrap] installing arm-none-eabi-gcc (user-local, xPack)"
  ARMGCC_DIR="$(python3 - <<'PY'
import json
import os
import re
import sys
import tarfile
import urllib.request

home = os.path.expanduser("~")
opt = os.path.join(home, ".local", "opt")
cache = os.path.join(home, ".cache", "edgeai_sand_demo")
os.makedirs(opt, exist_ok=True)
os.makedirs(cache, exist_ok=True)

api = "https://api.github.com/repos/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/latest"
with urllib.request.urlopen(api) as r:
    data = json.load(r)

assets = data.get("assets", [])
url = None
name = None
for a in assets:
    n = a.get("name", "")
    if re.search(r"xpack-arm-none-eabi-gcc-.*-linux-x64\.tar\.gz$", n):
        url = a.get("browser_download_url")
        name = n
        break

if not url:
    print("Could not find xPack arm-none-eabi-gcc linux-x64 asset in latest release.", file=sys.stderr)
    sys.exit(1)

tar_path = os.path.join(cache, name)
urllib.request.urlretrieve(url, tar_path)

with tarfile.open(tar_path, "r:gz") as tf:
    tf.extractall(path=opt)

dirs = [d for d in os.listdir(opt) if d.startswith("xpack-arm-none-eabi-gcc-")]
dirs.sort()
if not dirs:
    print("Extracted toolchain but could not find install directory.", file=sys.stderr)
    sys.exit(1)

install_dir = os.path.join(opt, dirs[-1])
print(install_dir)
PY
  )"
  export ARMGCC_DIR
  if [[ -d "$ARMGCC_DIR/bin" ]]; then
    export PATH="$ARMGCC_DIR/bin:$PATH"
  fi
fi

echo "[bootstrap] versions:"
python3 -V
python3 -m pip -V
west --version
cmake --version | head -n 1
ninja --version
arm-none-eabi-gcc --version | head -n 1 || true
git-lfs --version || true

echo "[bootstrap] done. Ensure PATH includes ~/.local/bin in new shells."

