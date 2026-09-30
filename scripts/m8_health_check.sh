#!/usr/bin/env bash
# Quick M8 CDC health check via /dev/ttyACM0 (no python-serial dep).
set -u
DEV=/dev/ttyACM0

stty -F "$DEV" 115200 raw -echo 2>/dev/null || { echo "stty failed"; exit 1; }
dd if="$DEV" of=/dev/null bs=1 count=4096 2>/dev/null  # drain
exec 3<>"$DEV" || exit 1
printf 'E' >&3
sleep 0.2
printf 'R' >&3
sleep 0.3
# non-blocking peek via dd with timeout wrapper
timeout 1 dd if="$DEV" of=/tmp/m8resp.bin bs=1 count=256 2>/dev/null
printf 'D' >&3
sleep 0.2
exec 3<&-
echo "bytes read: $(stat -c %s /tmp/m8resp.bin 2>/dev/null || echo 0)"
xxd /tmp/m8resp.bin 2>/dev/null | head -4
echo "=== check complete ==="
