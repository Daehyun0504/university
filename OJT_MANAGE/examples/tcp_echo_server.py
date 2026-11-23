"""4주차 TCP 실습: python3 examples/tcp_echo_server.py --port 9000"""

import argparse
import socket
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="TCP Echo 서버 (HTTP 서버 아님)")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9000)
    args = parser.parse_args()
    if not 0 <= args.port <= 65535:
        parser.error("포트는 0~65535 사이여야 합니다.")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind((args.host, args.port))
            server.listen(5)
            host, port = server.getsockname()
            print(f"Echo listening on {host}:{port}", flush=True)
            while True:
                connection, _ = server.accept()
                with connection:
                    connection.settimeout(5)
                    try:
                        while data := connection.recv(4096):
                            connection.sendall(data)
                    except OSError:
                        continue
    except KeyboardInterrupt:
        print("\nEcho 서버를 종료했습니다.")
    except OSError as exc:
        print(f"Echo 서버 실행 실패: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

