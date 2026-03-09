#!/usr/bin/env python3
"""
babixGO IPC Receiver
====================
Empfaengt Events vom nativen Modul via lokalem TCP-Socket.

Das Modul verbindet sich als CLIENT zu diesem Server.
Jede Nachricht ist newline-terminiert.

Einrichtung (einmalig, solange Geraet per USB verbunden):
    adb reverse tcp:27182 tcp:27182

Start:
    python scripts/ipc_receiver.py
    python scripts/ipc_receiver.py --port 27182

Port kann auch per Umgebungsvariable gesetzt werden:
    BABIX_IPC_PORT=27182 python scripts/ipc_receiver.py

Auf dem Geraet:
    Per Magisk-Modul wird BABIX_IPC_PORT automatisch aus der Umgebung gelesen,
    oder der Default-Port 27182 verwendet.
"""

import argparse
import os
import socket
import sys
from datetime import datetime


DEFAULT_PORT = 27182
HOST = "127.0.0.1"


def run(port: int) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as srv:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            srv.bind((HOST, port))
        except OSError as e:
            print(f"[ERROR] bind({HOST}:{port}) failed: {e}", file=sys.stderr)
            sys.exit(1)

        srv.listen(1)
        print(f"[babixGO IPC] Listening on {HOST}:{port}  (Ctrl+C to stop)")
        print(f"[babixGO IPC] Tipp: adb reverse tcp:{port} tcp:{port}")
        print()

        while True:
            try:
                conn, addr = srv.accept()
            except KeyboardInterrupt:
                print("\n[babixGO IPC] Stopped.")
                return

            print(f"[babixGO IPC] Module connected from {addr}")
            with conn:
                try:
                    for raw_line in conn.makefile(encoding="utf-8", errors="replace"):
                        line = raw_line.rstrip("\n\r")
                        if line:
                            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                            print(f"[{ts}] {line}")
                except (ConnectionResetError, BrokenPipeError):
                    pass
            print("[babixGO IPC] Module disconnected, waiting for next connection...")


def main() -> None:
    parser = argparse.ArgumentParser(description="babixGO native IPC receiver")
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.environ.get("BABIX_IPC_PORT", DEFAULT_PORT)),
        help=f"TCP port to listen on (default: {DEFAULT_PORT})",
    )
    args = parser.parse_args()
    run(args.port)


if __name__ == "__main__":
    main()
