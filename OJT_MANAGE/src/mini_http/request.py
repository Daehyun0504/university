"""수신한 HTTP 헤더를 해석한다. 네트워크 접속 없이 단위 시험할 수 있다."""

import re
from dataclasses import dataclass

TOKEN = re.compile(r"[!#$%&'*+.^_`|~0-9A-Za-z-]+")


class HTTPError(Exception):
    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status


@dataclass(frozen=True)
class Request:
    method: str
    target: str
    version: str
    headers: dict[str, str]

    @property
    def path(self) -> str:
        return self.target.partition("?")[0]


def parse_request(raw: bytes) -> Request:
    """CRLF로 끝나는 헤더를 파싱한다. 이 서버는 요청 본문을 지원하지 않는다."""
    if not raw.endswith(b"\r\n\r\n"):
        raise HTTPError(400, "헤더 종료 구분자가 없습니다.")
    lines = raw[:-4].decode("iso-8859-1").split("\r\n")
    parts = lines[0].split(" ")
    if len(parts) != 3 or not TOKEN.fullmatch(parts[0]):
        raise HTTPError(400, "요청 라인이 잘못되었습니다.")
    method, target, version = parts
    if not re.fullmatch(r"HTTP/\d\.\d", version):
        raise HTTPError(400, "HTTP 버전 형식이 잘못되었습니다.")
    if version not in {"HTTP/1.0", "HTTP/1.1"}:
        raise HTTPError(505, "HTTP/1.0과 HTTP/1.1만 지원합니다.")
    if (
        not target.startswith("/")
        or target.startswith("//")
        or "#" in target
        or any(ord(c) <= 32 or ord(c) >= 127 for c in target)
    ):
        raise HTTPError(400, "요청 대상은 /로 시작하는 ASCII URL이어야 합니다.")

    headers: dict[str, str] = {}
    for line in lines[1:]:
        name, separator, value = line.partition(":")
        if not separator or not TOKEN.fullmatch(name):
            raise HTTPError(400, "헤더 이름 또는 형식이 잘못되었습니다.")
        if any((ord(c) < 32 and c != "\t") or ord(c) == 127 for c in value):
            raise HTTPError(400, "헤더 값에 제어 문자가 있습니다.")
        name = name.lower()
        # 범위를 단순하게 유지하기 위해 동일 이름의 중복 헤더는 모두 거부한다.
        if name in headers:
            raise HTTPError(400, "중복 헤더를 지원하지 않습니다.")
        headers[name] = value.strip(" \t")

    if version == "HTTP/1.1" and not headers.get("host"):
        raise HTTPError(400, "HTTP/1.1 요청에는 Host 헤더가 필요합니다.")
    if "host" in headers:
        host = headers["host"]
        if not host or any(c.isspace() or c in "/\\,@?#" or ord(c) > 126 for c in host):
            raise HTTPError(400, "Host 헤더가 잘못되었습니다.")
    if "transfer-encoding" in headers and "content-length" in headers:
        raise HTTPError(400, "요청 본문 길이 정보가 충돌합니다.")
    if "transfer-encoding" in headers:
        raise HTTPError(501, "Transfer-Encoding을 지원하지 않습니다.")
    if "content-length" in headers:
        length = headers["content-length"]
        if not re.fullmatch(r"[0-9]+", length):
            raise HTTPError(400, "Content-Length가 잘못되었습니다.")
        # 큰 정수 변환 없이 0인지 검사한다.
        if length.strip("0"):
            raise HTTPError(400, "요청 본문을 지원하지 않습니다.")
    if "expect" in headers:
        raise HTTPError(417, "Expect 헤더를 지원하지 않습니다.")
    return Request(method, target, version, headers)

