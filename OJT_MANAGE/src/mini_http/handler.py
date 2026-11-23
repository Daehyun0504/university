"""URL을 문서 루트 안의 정적 파일에 연결한다."""

import mimetypes
import re
from pathlib import Path
from urllib.parse import unquote

from .config import MAX_FILE_BYTES
from .request import HTTPError, Request
from .response import Response, error_response

KNOWN_METHODS = {"HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH"}


def resolve_path(target: str, root: Path) -> Path:
    path = target.partition("?")[0]
    if re.search(r"%(?![0-9A-Fa-f]{2})", path):
        raise HTTPError(400, "URL 인코딩이 잘못되었습니다.")
    try:
        decoded = unquote(path, encoding="utf-8", errors="strict")
    except UnicodeDecodeError as exc:
        raise HTTPError(400, "URL 경로는 UTF-8이어야 합니다.") from exc
    if not decoded.startswith("/") or any(ord(c) < 32 or ord(c) == 127 for c in decoded):
        raise HTTPError(400, "URL 경로가 잘못되었습니다.")
    if "\\" in decoded:
        raise HTTPError(403, "허용하지 않는 경로입니다.")
    root = root.resolve()
    candidate = (root / ("index.html" if decoded == "/" else decoded.lstrip("/"))).resolve()
    if not candidate.is_relative_to(root):
        raise HTTPError(403, "문서 루트 밖으로 접근할 수 없습니다.")
    return candidate


def read_file(path: Path, limit: int) -> bytes:
    if not path.is_file():
        raise FileNotFoundError(path)
    with path.open("rb") as source:
        body = source.read(limit + 1)
    if len(body) > limit:
        raise HTTPError(403, "제공 가능한 파일 크기를 초과했습니다.")
    return body


def handle_request(request: Request, root: Path, max_file_bytes: int = MAX_FILE_BYTES) -> Response:
    if request.method != "GET":
        if request.method in KNOWN_METHODS:
            return error_response(405, headers={"Allow": "GET"})
        return error_response(501)
    try:
        path = resolve_path(request.target, root)
        body = read_file(path, max_file_bytes)
        guessed_type, encoding = mimetypes.guess_type(path.name)
        # .html.gz 등의 압축 파일을 HTML이라고 표시하지 않는다.
        # 압축 전송은 지원 범위 밖이므로 원본 바이트를 다운로드 유형으로 제공한다.
        content_type = "application/octet-stream" if encoding else (guessed_type or "application/octet-stream")
        if content_type.startswith("text/"):
            content_type += "; charset=utf-8"
        return Response(200, body, content_type)
    except (FileNotFoundError, NotADirectoryError):
        # 커스텀 404 페이지도 같은 경로 및 크기 제한을 적용한다.
        try:
            page = resolve_path("/404.html", root)
            return Response(404, read_file(page, max_file_bytes))
        except (OSError, HTTPError, RuntimeError):
            return error_response(404)
    except HTTPError as exc:
        return error_response(exc.status)
    except (PermissionError, RuntimeError):
        return error_response(403)
    except OSError:
        return error_response(500)
