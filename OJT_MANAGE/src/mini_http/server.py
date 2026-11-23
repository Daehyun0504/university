"""TCP 소켓으로 연결당 한 요청을 처리하는 순차 HTTP 서버."""

import logging
import socket
import threading
import time

from .config import ServerConfig
from .handler import handle_request
from .request import HTTPError, parse_request
from .response import error_response

LOGGER = logging.getLogger(__name__)


class HTTPServer:
    def __init__(self, config: ServerConfig):
        self.config = config
        self._listener: socket.socket | None = None
        self._stop = threading.Event()

    def __enter__(self) -> "HTTPServer":
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind((self.config.host, self.config.port))
            listener.listen(16)
            listener.settimeout(0.1)
        except BaseException:
            listener.close()
            raise
        self._listener = listener
        self._stop.clear()
        return self

    def __exit__(self, *_: object) -> None:
        self.shutdown()
        if self._listener is not None:
            self._listener.close()
            self._listener = None

    @property
    def address(self) -> tuple[str, int]:
        if self._listener is None:
            raise RuntimeError("서버가 아직 시작되지 않았습니다.")
        return self._listener.getsockname()

    def shutdown(self) -> None:
        """현재 요청 종료 후 accept 루프를 중지한다."""
        self._stop.set()

    def _receive_headers(self, connection: socket.socket) -> bytes | None:
        data = bytearray()
        deadline = time.monotonic() + self.config.request_timeout
        while True:
            end = data.find(b"\r\n\r\n")
            if end >= 0:
                if end + 4 > self.config.max_header_bytes:
                    raise HTTPError(431, "요청 헤더가 너무 큽니다.")
                # Keep-Alive와 파이프라이닝은 처리하지 않고 첫 요청 후 종료한다.
                return bytes(data[:end + 4])
            if len(data) >= self.config.max_header_bytes:
                raise HTTPError(431, "요청 헤더가 너무 큽니다.")
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise HTTPError(408, "요청 수신 시간이 초과되었습니다.")
            connection.settimeout(remaining)
            try:
                chunk = connection.recv(min(4096, self.config.max_header_bytes - len(data)))
            except TimeoutError as exc:
                raise HTTPError(408, "요청 수신 시간이 초과되었습니다.") from exc
            if not chunk:
                if not data:
                    return None
                raise HTTPError(400, "요청 헤더 수신 중 연결이 끝났습니다.")
            data.extend(chunk)

    def _handle_connection(self, connection: socket.socket, peer: tuple[str, int]) -> None:
        version = "HTTP/1.1"
        head_only = False
        try:
            raw = self._receive_headers(connection)
            if raw is None:
                return
            head_only = raw.startswith(b"HEAD ")
            request = parse_request(raw)
            version = request.version
            response = handle_request(request, self.config.document_root, self.config.max_file_bytes)
        except HTTPError as exc:
            response = error_response(exc.status)
        connection.settimeout(self.config.request_timeout)
        connection.sendall(response.to_bytes(version, head_only=head_only))
        LOGGER.info("%s:%d -> %d", peer[0], peer[1], response.status)

    def serve_forever(self) -> None:
        listener = self._listener
        if listener is None:
            raise RuntimeError("with HTTPServer(config) 안에서 실행하세요.")
        while not self._stop.is_set():
            try:
                connection, peer = listener.accept()
            except TimeoutError:
                continue
            with connection:
                try:
                    self._handle_connection(connection, peer)
                except OSError:
                    # 클라이언트 중도 종료나 쓰기 타임아웃은 다음 연결에 영향을 주지 않는다.
                    LOGGER.debug("클라이언트 연결 종료: %s", peer, exc_info=True)

