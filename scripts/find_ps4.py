#!/usr/bin/env python3
"""Find the PS4 on the LAN by its GoldHEN FTP port (2121) / binloader (9090)."""
import socket
import concurrent.futures

PORTS = (2121, 9090)
TIMEOUT = 0.4

def probe(args):
    ip, port = args
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(TIMEOUT)
    try:
        s.connect((ip, port))
        banner = b""
        if port == 2121:
            try:
                banner = s.recv(64)
            except OSError:
                pass
        return (ip, port, banner.decode("latin1").strip())
    except OSError:
        return None
    finally:
        s.close()

def main():
    targets = [(f"192.168.0.{i}", p) for i in range(1, 255) for p in PORTS]
    hits = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=200) as ex:
        for r in ex.map(probe, targets):
            if r:
                hits.append(r)
    for ip, port, banner in sorted(hits):
        print(f"{ip}:{port}  {banner!r}")
    if not hits:
        print("no console found - is it on and in a HEN-active state?")

if __name__ == "__main__":
    main()
