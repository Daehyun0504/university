from urllib.parse import quote

import pytest

from mini_http.handler import handle_request
from mini_http.request import parse_request


def request(path="/", method="GET"):
    return parse_request(f"{method} {path} HTTP/1.0\r\n\r\n".encode())


@pytest.fixture
def root(tmp_path):
    directory = tmp_path / "www"
    directory.mkdir()
    (directory / "index.html").write_text("<html>안녕하세요</html>", encoding="utf-8")
    (directory / "404.html").write_text("없는 페이지", encoding="utf-8")
    return directory


def test_index_and_query(root):
    result = handle_request(request("/?lesson=6"), root)
    assert result.status == 200
    assert result.body == (root / "index.html").read_bytes()
    assert result.content_type == "text/html; charset=utf-8"


def test_encoded_utf8_filename(root):
    (root / "한글.txt").write_text("한글 경로", encoding="utf-8")
    result = handle_request(request("/" + quote("한글.txt")), root)
    assert result.status == 200
    assert result.body.decode() == "한글 경로"


@pytest.mark.parametrize("suffix,mime", [
    (".css", "text/css; charset=utf-8"),
    (".png", "image/png"),
    (".svg", "image/svg+xml"),
    (".ojt_unknown", "application/octet-stream"),
    (".html.gz", "application/octet-stream"),
])
def test_mime_and_binary_body(root, suffix, mime):
    body = b"body" if suffix == ".css" else bytes(range(256))
    (root / ("asset" + suffix)).write_bytes(body)
    result = handle_request(request("/asset" + suffix), root)
    assert (result.status, result.content_type, result.body) == (200, mime, body)


def test_custom_and_fallback_404(root):
    assert handle_request(request("/missing"), root).body.decode() == "없는 페이지"
    (root / "404.html").unlink()
    fallback = handle_request(request("/missing"), root)
    assert fallback.status == 404
    assert b"404 Not Found" in fallback.body


def test_directory_listing_is_not_provided(root):
    (root / "private").mkdir()
    assert handle_request(request("/private/"), root).status == 404


@pytest.mark.parametrize("path,status", [
    ("/../secret.txt", 403),
    ("/%2e%2e/secret.txt", 403),
    ("/..%2fsecret.txt", 403),
    ("/..%5csecret.txt", 403),
    ("/%00", 400),
    ("/%0a", 400),
    ("/%ZZ", 400),
    ("/%ff", 400),
])
def test_invalid_paths(root, path, status):
    (root.parent / "secret.txt").write_text("secret")
    result = handle_request(request(path), root)
    assert result.status == status
    assert b"secret" not in result.body


def test_symlink_escape_and_custom_error_escape(root):
    secret = root.parent / "secret.txt"
    secret.write_text("never expose this")
    (root / "link.txt").symlink_to(secret)
    assert handle_request(request("/link.txt"), root).status == 403
    (root / "404.html").unlink()
    (root / "404.html").symlink_to(secret)
    result = handle_request(request("/missing"), root)
    assert result.status == 404
    assert b"never expose this" not in result.body


def test_file_size_limit(root):
    (root / "large.bin").write_bytes(b"x" * 65)
    assert handle_request(request("/large.bin"), root, 64).status == 403


@pytest.mark.parametrize("method,status", [("POST", 405), ("HEAD", 405), ("UNKNOWN", 501)])
def test_unsupported_methods(root, method, status):
    response = handle_request(request(method=method), root)
    assert response.status == status
    if status == 405:
        assert response.headers["Allow"] == "GET"
