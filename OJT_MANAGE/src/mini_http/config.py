"""기본 설정과 실행 옵션. 파일 제공 범위는 document_root 아래로 제한한다."""

import argparse
import math
from dataclasses import dataclass
from pathlib import Path

HOST = "127.0.0.1"
PORT = 8000
DOCUMENT_ROOT = Path(__file__).resolve().parents[2] / "www"
REQUEST_TIMEOUT = 5.0
MAX_HEADER_BYTES = 16 * 1024
MAX_FILE_BYTES = 8 * 1024 * 1024


@dataclass(frozen=True)
class ServerConfig:
    host: str = HOST
    port: int = PORT
    document_root: Path = DOCUMENT_ROOT
    request_timeout: float = REQUEST_TIMEOUT
    max_header_bytes: int = MAX_HEADER_BYTES
    max_file_bytes: int = MAX_FILE_BYTES

    def __post_init__(self) -> None:
        root = Path(self.document_root).resolve()
        if not root.is_dir():
            raise ValueError(f"문서 루트 디렉터리가 없습니다: {root}")
        if not 0 <= self.port <= 65535:
            raise ValueError("포트는 0~65535 사이여야 합니다.")
        if not math.isfinite(self.request_timeout) or self.request_timeout <= 0:
            raise ValueError("요청 제한 시간은 유한한 양수여야 합니다.")
        if self.max_header_bytes <= 0 or self.max_file_bytes <= 0:
            raise ValueError("헤더와 파일 크기 제한은 양수여야 합니다.")
        object.__setattr__(self, "document_root", root)


def parse_config(argv: list[str] | None = None) -> ServerConfig:
    parser = argparse.ArgumentParser(description="Socket 기반 미니 HTTP 서버")
    parser.add_argument("--host", default=HOST, help="바인딩 주소 (기본: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=PORT, help="포트 (기본: 8000, 0: 자동 할당)")
    parser.add_argument("--root", type=Path, default=DOCUMENT_ROOT, help="정적 파일 디렉터리")
    parser.add_argument("--timeout", type=float, default=REQUEST_TIMEOUT, help="요청 헤더 수신 제한 시간(초)")
    parser.add_argument("--max-header-bytes", type=int, default=MAX_HEADER_BYTES)
    parser.add_argument("--max-file-bytes", type=int, default=MAX_FILE_BYTES)
    args = parser.parse_args(argv)
    try:
        return ServerConfig(
            host=args.host,
            port=args.port,
            document_root=args.root,
            request_timeout=args.timeout,
            max_header_bytes=args.max_header_bytes,
            max_file_bytes=args.max_file_bytes,
        )
    except ValueError as exc:
        parser.error(str(exc))

