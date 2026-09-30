#!/usr/bin/env bash
# Fetch and print the probe log from the PS4 (run after the probe has executed).
set -euo pipefail

IP="${1:-}"
if [ -z "$IP" ]; then
  IP=$(python3 "$(dirname "$0")/find_ps4.py" | grep 2121 | head -1 | cut -d: -f1)
fi
echo ">> PS4 at $IP"
if curl -s --list-only "ftp://$IP:2121/data/m8c_probe.log" >/dev/null 2>&1; then
  curl -s "ftp://$IP:2121/data/m8c_probe.log" -o /tmp/m8c_probe.log
  echo ">> /data/m8c_probe.log:"
  cat /tmp/m8c_probe.log
else
  echo ">> no /data/m8c_probe.log yet - install and run the probe first"
  echo ">> (Settings -> Debug Settings -> Game -> Package Installer,"
  echo ">>  then launch 'm8c PS4 Usbd Probe')"
  exit 1
fi
