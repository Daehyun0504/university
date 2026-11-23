"""HTTP 응답을 바이트로 조립한다. 소켓 전송은 server.py가 담당한다."""

from dataclasses import dataclass, field
from email.utils import formatdate
from http import HTTPStatus


@dataclass(frozen=True)
class Response:
    status: int
    body: bytes = b""
    content_type: str = "text/html; charset=utf-8"
    headers: dict[str, str] = field(default_factory=dict)

    def to_bytes(self, version: str = "HTTP/1.1", *, head_only: bool = False) -> bytes:
        if version not in {"HTTP/1.0", "HTTP/1.1"}:
            raise ValueError("지원하지 않는 응답 버전입니다.")
        status = HTTPStatus(self.status)
        headers = {
            "Date": formatdate(usegmt=True),
            "Server": "mini_http/0.1",
            "Content-Type": self.content_type,
            "Content-Length": str(len(self.body)),
            "Connection": "close",
            "X-Content-Type-Options": "nosniff",
        }
        for name, value in self.headers.items():
            if name.lower() in {key.lower() for key in headers}:
                raise ValueError("기본 응답 헤더를 덮어쓸 수 없습니다.")
            if any(c in name + value for c in "\r\n"):
                raise ValueError("응답 헤더에 줄바꿈을 사용할 수 없습니다.")
            headers[name] = value
        lines = [f"{version} {status.value} {status.phrase}"]
        lines.extend(f"{name}: {value}" for name, value in headers.items())
        header = ("\r\n".join(lines) + "\r\n\r\n").encode("iso-8859-1")
        return header if head_only else header + self.body


def error_response(status: int, *, headers: dict[str, str] | None = None) -> Response:
    reason = HTTPStatus(status).phrase
    body = (
        '<!doctype html><html lang="ko"><meta charset="utf-8">'
        f"<title>{status} {reason}</title><h1>{status} {reason}</h1>"
        "<p>요청을 처리할 수 없습니다.</p></html>"
    ).encode("utf-8")
    return Response(status, body, headers=headers or {})

