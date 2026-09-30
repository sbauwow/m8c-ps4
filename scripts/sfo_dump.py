#!/usr/bin/env python3
"""Dump PS4 param.sfo key/values."""
import struct
import sys

def dump(path):
    data = open(path, "rb").read()
    magic, version, _pad, key_table_start, data_table_start, entries_count = struct.unpack_from(
        "<4sHHIII", data, 0
    )
    print(f"== {path} (magic {magic.decode()} ver 0x{version:04x}) ==")
    for i in range(entries_count):
        off = 20 + i * 16
        key_offset, data_fmt, data_len, data_max, data_offset = struct.unpack_from("<HHIII", data, off)
        key_start = key_table_start + key_offset
        key = data[key_start:data.index(b"\0", key_start)].decode()
        val_start = data_table_start + data_offset
        raw = data[val_start:val_start + data_len]
        if data_fmt in (0x0400, 0x0104):  # utf8 special/simplified
            val = raw.split(b"\0")[0].decode(errors="replace")
        elif data_fmt == 0x0004:  # int32
            val = struct.unpack("<i", raw[:4])[0]
        else:
            val = raw.hex()
        print(f"  {key:24s} = {val}")

for p in sys.argv[1:]:
    dump(p)
