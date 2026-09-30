#!/usr/bin/env bash
# Build + deploy the m8c PS4 pkg (and optionally the probe) to the PS4.
# Usage: deploy.sh [ps4-ip]   (default: auto-find via find_ps4.py, banner-checked)
set -euo pipefail
cd "$(dirname "$0")/.."

PS4_IP="${1:-}"
if [ -z "$PS4_IP" ]; then
  # find_ps4.py prints ip:port banner; keep only the GoldHEN PS4 banner.
  PS4_IP=$(python3 scripts/find_ps4.py | grep 'GoldHEN FTP' | head -1 | cut -d: -f1)
fi
echo ">> PS4 at $PS4_IP"

export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$HOME/ps4-toolchain/OpenOrbis/PS4Toolchain}"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
export LD_LIBRARY_PATH="$HOME/ps5-jailbreak/ps4-transfer/pkgtool/openssl11/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="/usr/bin:$PATH"   # theos clang-11 trap

PKG=$(ls UP0001-MBCA00001_*.pkg 2>/dev/null | head -1 || true)
if [ -z "${PKG}" ] || [ -n "$(find app port Makefile -newer "$PKG" -print -quit 2>/dev/null)" ]; then
  echo ">> building..."
  make clean >/dev/null
  make
  PKG=$(ls UP0001-MBCA00001_*.pkg | head -1)
fi

echo ">> uploading $PKG"
curl -sS --ftp-create-dirs -T "$PKG" "ftp://$PS4_IP:2121/data/pkg/"

# Verify byte-exactness (FTP has silently no-op'd before when the dir was missing).
LOCAL_HASH=$(sha256sum "$PKG" | awk '{print $1}')
CHECK=$(mktemp)
trap 'rm -f "$CHECK"' EXIT
REMOTE_HASH=$(curl -sS "ftp://$PS4_IP:2121/data/pkg/$PKG" -o "$CHECK" && sha256sum "$CHECK" | awk '{print $1}')
if [ "$LOCAL_HASH" = "$REMOTE_HASH" ]; then
  echo ">> OK: $PKG is on the PS4 (hash verified)."
  echo ">> Install: Settings -> Debug Settings -> Game -> Package Installer"
else
  echo ">> HASH MISMATCH - upload corrupted, aborting"
  exit 1
fi
