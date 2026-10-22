#!/usr/bin/env python3
"""Test helper: forwards 127.0.0.1:<listen> to 127.0.0.1:<target>, but drops the very first connection.
Used by audit-runtime.sh to check that HopCast retries when the default connection fails. GPL-2.0-or-later."""
import socket
import sys
import threading

listen, target = int(sys.argv[1]), int(sys.argv[2])
srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", listen))
srv.listen(8)


def pipe(a, b):
    try:
        while True:
            data = a.recv(65536)
            if not data:
                break
            b.sendall(data)
    except OSError:
        pass
    finally:
        for s in (a, b):
            try:
                s.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass


first = True
while True:
    client, _ = srv.accept()
    if first:
        first = False
        print("dropped the first connection", flush=True)
        client.close()
        continue
    try:
        upstream = socket.create_connection(("127.0.0.1", target), timeout=10)
    except OSError:
        client.close()
        continue
    threading.Thread(target=pipe, args=(client, upstream), daemon=True).start()
    threading.Thread(target=pipe, args=(upstream, client), daemon=True).start()
