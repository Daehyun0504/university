"""python3 -m mini_http 실행 진입점."""

import logging
import sys

from .config import parse_config
from .server import HTTPServer


def main(argv: list[str] | None = None) -> int:
    config = parse_config(argv)
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")
    try:
        with HTTPServer(config) as server:
            host, port = server.address
            print(f"Listening on {host}:{port} (root: {config.document_root})", flush=True)
            print(f"Local URL: http://127.0.0.1:{port}/" if host == "0.0.0.0" else f"URL: http://{host}:{port}/", flush=True)
            server.serve_forever()
    except KeyboardInterrupt:
        print("\n서버를 종료했습니다.")
    except OSError as exc:
        print(f"서버 실행 실패: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

