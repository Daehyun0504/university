from pathlib import Path

import pytest

from mini_http.request import HTTPError, parse_request

FIXTURES = Path(__file__).resolve().parents[1] / "fixtures"


def test_browser_request_fixture():
    request = parse_request((FIXTURES / "valid_get.txt").read_bytes())
    assert (request.method, request.path, request.version) == ("GET", "/", "HTTP/1.1")
    assert request.headers["host"] == "localhost:8000"


def test_http10_without_host_and_query():
    request = parse_request(b"GET /index.html?lesson=5 HTTP/1.0\r\n\r\n")
    assert request.path == "/index.html"
    assert request.target == "/index.html?lesson=5"


def test_header_case_and_whitespace():
    request = parse_request(b"GET / HTTP/1.1\r\nhOsT:\tlocalhost \t\r\nContent-Length: 00\r\n\r\n")
    assert request.headers["host"] == "localhost"


@pytest.mark.parametrize("raw,status", [
    (b"GET / HTTP/1.1\r\n\r\n", 400),
    (b"GET / HTTP/1.1\r\nHost: a\r\nhost: b\r\n\r\n", 400),
    (b"GET  / HTTP/1.0\r\n\r\n", 400),
    (b"GET / HTTP/2.0\r\n\r\n", 505),
    (b"GET / HTTP/no\r\n\r\n", 400),
    (b"GET http://localhost/ HTTP/1.0\r\n\r\n", 400),
    (b"GET //localhost/ HTTP/1.0\r\n\r\n", 400),
    (b"GET /#fragment HTTP/1.0\r\n\r\n", 400),
    (b"GET /\x00 HTTP/1.0\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nBad Header: a\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nHost : a\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nHost: a\x00b\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nHost: a/b\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nHost: a\r\n continuation\r\n\r\n", 400),
    (b"GET / HTTP/1.0\n\n", 400),
    (b"GET / HTTP/1.0\r\nContent-Length: -1\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nContent-Length: 10\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n", 501),
    (b"GET / HTTP/1.0\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nExpect: 100-continue\r\n\r\n", 417),
])
def test_invalid_or_unsupported_requests(raw, status):
    with pytest.raises(HTTPError) as result:
        parse_request(raw)
    assert result.value.status == status


def test_malformed_fixture():
    with pytest.raises(HTTPError) as result:
        parse_request((FIXTURES / "malformed_request.txt").read_bytes())
    assert result.value.status == 400


def test_very_long_content_length_is_a_client_error():
    with pytest.raises(HTTPError) as result:
        parse_request(b"GET / HTTP/1.0\r\nContent-Length: " + b"9" * 5000 + b"\r\n\r\n")
    assert result.value.status == 400

