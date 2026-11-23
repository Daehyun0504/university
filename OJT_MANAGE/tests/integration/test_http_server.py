import os
import signal
import socket
import subprocess
import sys
import threading
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import ProxyHandler, build_opener

import pytest

from mini_http.config import ServerConfig
from mini_http.server import HTTPServer

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture
def running_server(tmp_path):
    (tmp_path / "index.html").write_text("<html>안녕하세요</html>", encoding="utf-8")
    (tmp_path / "404.html").write_text("찾을 수 없습니다", encoding="utf-8")
    (tmp_path / "sample.bin").write_bytes(bytes(range(256)))
    config = ServerConfig(port=0, document_root=tmp_path, request_timeout=0.25, max_header_bytes=1024)
    # __enter__가 bind/listen을 끝내므로 별도 sleep이나 포트 탐색 경쟁이 없다.
    with HTTPServer(config) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            yield server
        finally:
            server.shutdown()
            thread.join(timeout=2)
            assert not thread.is_alive(), "시험용 서버가 종료되지 않았습니다."


def receive_all(client):
    chunks = []
    while chunk := client.recv(4096):
        chunks.append(chunk)
    return b"".join(chunks)


def exchange(server, raw):
    with socket.create_connection(server.address, timeout=2) as client:
        client.sendall(raw)
        return receive_all(client)


def status_of(raw):
    return int(raw.split(b" ", 2)[1])


def test_get_with_http_client(running_server):
    host, port = running_server.address
    opener = build_opener(ProxyHandler({}))
    with opener.open(f"http://{host}:{port}/", timeout=2) as response:
        body = response.read()
        assert response.status == 200
        assert body.decode() == "<html>안녕하세요</html>"
        assert int(response.headers["Content-Length"]) == len(body)
        assert response.headers["Connection"] == "close"
    with pytest.raises(HTTPError) as error:
        opener.open(f"http://{host}:{port}/missing", timeout=2)
    with error.value as response:
        assert response.code == 404
        assert response.read().decode() == "찾을 수 없습니다"


def test_fragmented_headers(running_server):
    with socket.create_connection(running_server.address, timeout=2) as client:
        client.sendall(b"GET / HTTP/1.1\r\nHo")
        # 요청이 완성되기 전에 응답하지 않아야 한다.
        client.settimeout(0.03)
        with pytest.raises(TimeoutError):
            client.recv(1)
        client.settimeout(2)
        client.sendall(b"st: localhost\r\n\r\n")
        assert status_of(receive_all(client)) == 200


def test_http10_and_binary(running_server):
    wire = exchange(running_server, b"GET /sample.bin HTTP/1.0\r\n\r\n")
    assert wire.startswith(b"HTTP/1.0 200 OK")
    assert wire.split(b"\r\n\r\n", 1)[1] == bytes(range(256))


def test_keep_alive_request_still_closes_after_first_response(running_server):
    wire = exchange(running_server, b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n")
    assert status_of(wire) == 200
    assert b"Connection: close\r\n" in wire


@pytest.mark.parametrize("raw,status", [
    (b"GET / HTTP/1.1\r\n\r\n", 400),
    (b"POST / HTTP/1.0\r\n\r\n", 405),
    (b"UNKNOWN / HTTP/1.0\r\n\r\n", 501),
    (b"GET /%2e%2e/secret HTTP/1.0\r\n\r\n", 403),
    (b"GET / HTTP/2.0\r\n\r\n", 505),
    (b"GET / HTTP/1.0\r\nExpect: 100-continue\r\n\r\n", 417),
    (b"GET / HTTP/1.0\r\nContent-Length: 1\r\n\r\n", 400),
    (b"GET / HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n", 501),
])
def test_errors_and_server_survival(running_server, raw, status):
    assert status_of(exchange(running_server, raw)) == status
    assert status_of(exchange(running_server, b"GET / HTTP/1.0\r\n\r\n")) == 200


def test_head_error_does_not_include_body(running_server):
    wire = exchange(running_server, b"HEAD / HTTP/1.0\r\n\r\n")
    assert status_of(wire) == 405
    assert b"Allow: GET\r\n" in wire
    assert wire.split(b"\r\n\r\n", 1)[1] == b""


def test_header_limit(running_server):
    # 상한 크기까지 헤더가 끝나지 않으면 431을 반환한다.
    prefix = b"GET / HTTP/1.0\r\nX-Padding: "
    raw = prefix + b"x" * (1024 - len(prefix))
    assert status_of(exchange(running_server, raw)) == 431


def test_header_at_exact_limit(running_server):
    prefix = b"GET / HTTP/1.0\r\nX-Padding: "
    raw = prefix + b"x" * (1024 - len(prefix) - 4) + b"\r\n\r\n"
    assert status_of(exchange(running_server, raw)) == 200


def test_request_timeout_and_recovery(running_server):
    with socket.create_connection(running_server.address, timeout=2) as client:
        client.sendall(b"GET / HTTP/1.1\r\n")
        assert status_of(receive_all(client)) == 408
    assert status_of(exchange(running_server, b"GET / HTTP/1.0\r\n\r\n")) == 200


def test_incomplete_request_and_empty_disconnect(running_server):
    with socket.create_connection(running_server.address, timeout=2) as client:
        client.sendall(b"GET / HTTP/1.0\r\n")
        client.shutdown(socket.SHUT_WR)
        assert status_of(receive_all(client)) == 400
    with socket.create_connection(running_server.address, timeout=2) as client:
        client.shutdown(socket.SHUT_WR)
        assert receive_all(client) == b""
    assert status_of(exchange(running_server, b"GET / HTTP/1.0\r\n\r\n")) == 200


def test_cli_module_entrypoint_from_another_directory(tmp_path):
    # 배포 페이지와 기본 문서 루트가 현재 작업 폴더에 의존하지 않는지도 확인한다.
    env = {**os.environ, "PYTHONPATH": str(ROOT / "src")}
    process = subprocess.Popen(
        [sys.executable, "-m", "mini_http", "--port", "0"],
        cwd=tmp_path, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
    )
    try:
        import selectors
        with selectors.DefaultSelector() as selector:
            selector.register(process.stdout, selectors.EVENT_READ)
            assert selector.select(timeout=5), "서버 시작 로그가 없습니다."
        line = process.stdout.readline()
        assert line.startswith("Listening on 127.0.0.1:"), line
        port = int(line.split(":", 1)[1].split()[0])
        opener = build_opener(ProxyHandler({}))
        with opener.open(f"http://127.0.0.1:{port}/", timeout=2) as response:
            assert "미니 HTTP 서버" in response.read().decode()
        with opener.open(f"http://127.0.0.1:{port}/assets/css/style.css", timeout=2) as response:
            assert response.headers["Content-Type"] == "text/css; charset=utf-8"
        process.send_signal(signal.SIGINT)
        _, stderr = process.communicate(timeout=5)
        assert process.returncode == 0, stderr
    finally:
        if process.poll() is None:
            process.kill()
        process.communicate(timeout=5)


@pytest.mark.parametrize("args", [["--port", "-1"], ["--timeout", "nan"], ["--timeout", "0"], ["--max-header-bytes", "0"], ["--root", "/missing-ojt-document-root"]])
def test_invalid_cli_configuration(args):
    result = subprocess.run(
        [sys.executable, "-m", "mini_http", *args], cwd=ROOT,
        env={**os.environ, "PYTHONPATH": str(ROOT / "src")},
        capture_output=True, text=True, timeout=5,
    )
    assert result.returncode == 2
    assert "error:" in result.stderr
    assert "Traceback" not in result.stderr
