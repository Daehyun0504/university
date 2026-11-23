import pytest

from mini_http.response import Response, error_response


def test_utf8_content_length_counts_bytes():
    body = "안녕하세요".encode("utf-8")
    headers, actual = Response(200, body).to_bytes().split(b"\r\n\r\n", 1)
    assert headers.startswith(b"HTTP/1.1 200 OK\r\n")
    assert b"Content-Length: 15" in headers
    assert b"Content-Type: text/html; charset=utf-8" in headers
    assert b"Connection: close" in headers
    assert actual == body


def test_binary_content_is_not_reencoded():
    body = bytes(range(256))
    response = Response(200, body, "application/octet-stream").to_bytes("HTTP/1.0")
    assert response.startswith(b"HTTP/1.0 200 OK")
    assert response.split(b"\r\n\r\n", 1)[1] == body


def test_head_error_sends_headers_without_body():
    response = error_response(405, headers={"Allow": "GET"})
    wire = response.to_bytes(head_only=True)
    assert wire.endswith(b"\r\n\r\n")
    assert b"Allow: GET\r\n" in wire
    assert f"Content-Length: {len(response.body)}".encode() in wire
    assert wire.split(b"\r\n\r\n", 1)[1] == b""


@pytest.mark.parametrize("headers", [
    {"X-Test": "ok\r\nInjected: yes"},
    {"Content-Length": "0"},
    {"connection": "keep-alive"},
])
def test_reject_invalid_or_reserved_extra_headers(headers):
    with pytest.raises(ValueError):
        Response(200, b"hello", headers=headers).to_bytes()

