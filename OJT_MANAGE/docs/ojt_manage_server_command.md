# 서버 실행 및 테스트 명령어

기준 구조: [디렉터리 구조도](../ojt_manage_dir_diagram.md)

Linux Bash 환경에서 구현된 서버·테스트·Echo 실습 코드를 사용하는 방법이다. 서버는 표준 라이브러리만으로 실행할 수 있으며, 가상환경과 pytest는 자동 시험에 사용한다. 예시 서버 주소는 `127.0.0.1`, 포트는 `8000`이다.

## 1. 최초 환경 준비

Python 3.10 이상과 가상환경 생성 기능이 설치된 환경에서 실행한다. 이 작업 환경에는 `.venv`가 준비되어 있으므로 기존 환경 활성화부터 시작해도 된다.

```bash
cd /home/yoondh/work/OJT_MANAGE
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -e '.[test]'
```

`.venv`는 프로젝트 전용 Python 환경이다. 생성과 pytest 설치는 최초 한 번 수행하고, 새 터미널에서는 아래처럼 기존 환경을 활성화한다. `.venv/`는 `.gitignore`에 등록한다. [Python 가상환경 문서](https://docs.python.org/3/library/venv.html)

```bash
cd /home/yoondh/work/OJT_MANAGE
source .venv/bin/activate
```

서버는 Python 표준 라이브러리로 구현하고, `pytest`는 개발 중 자동 테스트에 사용한다.

Ubuntu/Debian에서 가상환경 생성 중 `ensurepip` 오류가 나면 `sudo apt install python3-venv` 후 생성 명령을 다시 실행한다. 시스템 pip가 설치되어 있는 경우에는 `python3 -m venv --without-pip .venv` 후 `python3 -m pip --python .venv install pip`로 준비하는 방법도 있다.

## 2. HTTP 서버 실행

실행 스크립트를 사용하면 어느 폴더에서든 서버를 시작할 수 있다. 프로젝트의 `.venv/bin/python3`가 있으면 사용하고, 없으면 시스템 `python3`를 사용한다.

```bash
/home/yoondh/work/OJT_MANAGE/docs/ojt_manage_run_server

# 다른 포트 사용 또는 옵션 확인
/home/yoondh/work/OJT_MANAGE/docs/ojt_manage_run_server --port 8080
/home/yoondh/work/OJT_MANAGE/docs/ojt_manage_run_server --help
```

기본 주소는 `http://127.0.0.1:8000/`이며 `Ctrl+C`로 종료한다. 서버 옵션은 그대로 전달되며, `--root`에 상대 경로를 지정하면 스크립트를 호출한 작업 폴더 기준으로 해석한다.

`src/mini_http/__main__.py`가 설정을 읽고 서버 시작 함수를 호출한다. 프로젝트 루트에서 실행한다.

```bash
PYTHONPATH=src python3 -m mini_http
```

| 부분 | 의미 |
| --- | --- |
| `PYTHONPATH=src` | 이번 명령에서 Python이 패키지를 찾는 경로에 현재 위치의 `src/`를 추가 |
| `python3` | Python 3 실행 |
| `-m mini_http` | `mini_http` 패키지를 찾아 `__main__.py` 실행 |

제안된 일반 패키지 구조에서는 `__init__.py`로 패키지를 초기화한 뒤 `__main__.py`를 실행한다. `__init__.py`에는 패키지 설명만 두고 서버 시작은 `__main__.py`에서 담당한다. `mini_http`는 직접 만드는 패키지 이름이다.

서버가 실행되는 터미널을 유지하고, 종료하려면 `Ctrl+C`를 누른다.

```bash
# 다른 포트·문서 루트·제한 시간 사용
PYTHONPATH=src python3 -m mini_http --port 8080 --root ./www --timeout 3

# 옵션 목록
PYTHONPATH=src python3 -m mini_http --help
```

헤더 크기는 `--max-header-bytes`(기본 16384), 파일 크기는 `--max-file-bytes`(기본 8388608)로 제한한다. 기본 문서 루트는 소스 기준 프로젝트의 `www/`이며, 일반 wheel 설치로 사용할 때는 `--root`로 파일 위치를 지정한다.

## 3. 클라이언트로 로컬 접속

상세 접속 예시는 [Linux 클라이언트 접속 명령어](./ojt_manage_client_command.md)에 정리한다.

서버가 실행 중일 때 **다른 터미널**에서 실행한다. `curl`이 설치되어 있어야 하며, curl 사용에는 Python 가상환경 활성화가 필요하지 않다.

```bash
# 정상 경로: 상태 줄·헤더·본문 확인
curl -i --max-time 5 http://127.0.0.1:8000/

# 없는 경로: 404 응답 확인
curl -i --max-time 5 http://127.0.0.1:8000/not-found
```

`-i`는 본문과 함께 응답 헤더를 표시하고, `--max-time 5`는 요청 수행 시간을 최대 5초로 제한한다. 첫 요청은 `200`과 `www/index.html`의 내용을, 두 번째 요청은 `404`를 반환한다. curl의 출력은 사람이 확인하는 접속 시험이며, 위 명령 자체가 기대 상태 코드와 일치하는지 자동 판정하지는 않는다.

GUI 없이도 HTTP 요청·응답을 확인할 수 있다. HTML 화면을 보려면 같은 컴퓨터의 브라우저 주소창에서 `http://127.0.0.1:8000/`을 연다. `www/index.html` 파일을 직접 여는 방식은 서버 접속 시험이 아니다.

## 4. 다른 컴퓨터 또는 스마트폰에서 접속

코드 수정 없이 다음 옵션으로 LAN 접속을 허용할 수 있다.

```bash
PYTHONPATH=src python3 -m mini_http --host 0.0.0.0 --port 8000
```

또는 `src/mini_http/config.py`의 기본값을 변경하고 재시작한다.

```python
HOST = "0.0.0.0"
PORT = 8000
```

설정을 바꾼 뒤 서버를 재시작한다. 같은 LAN 또는 Wi-Fi의 다른 기기에서 서버 컴퓨터의 실제 LAN IP로 접속한다. 예를 들어 서버 IP가 `192.168.0.10`인 경우:

```bash
curl -i --max-time 5 http://192.168.0.10:8000/
```

스마트폰에서는 같은 주소를 브라우저에 입력한다. `0.0.0.0`은 서버의 연결 수신 설정이고 접속 주소가 아니다. 다른 기기에서 `127.0.0.1`을 입력하면 그 기기 자신을 가리킨다. 접속이 안 되면 서버의 바인딩 주소·포트, 방화벽, 공유기의 기기 간 통신 제한을 확인한다.

## 5. 자동 테스트 실행

프로젝트 루트에서 가상환경을 활성화하고 실행한다. `pyproject.toml`에도 `src` 탐색 경로가 설정되어 있다.

```bash
# 전체 테스트
PYTHONPATH=src python3 -m pytest tests/ -v

# 서버 내부 함수를 직접 검사하는 단위 테스트만 실행
PYTHONPATH=src python3 -m pytest tests/unit/ -v

# 실제 연결을 사용하는 통합 테스트만 실행
PYTHONPATH=src python3 -m pytest tests/integration/ -v

# 특정 파일만 실행
PYTHONPATH=src python3 -m pytest tests/unit/test_request.py -v

# 테스트 본문을 실행하지 않고 수집되는 테스트 목록 확인
PYTHONPATH=src python3 -m pytest tests/ --collect-only -q
```

`-v`는 테스트별 결과를 자세히 표시한다. `--collect-only`는 테스트 수집까지만 수행한다. 파일을 불러오는 과정은 있으므로 모듈 최상위에 서버 시작 같은 작업을 넣지 않는다. [pytest 실행 문서](https://docs.pytest.org/en/stable/how-to/usage.html)

| 구분 | 동작 | 서버를 먼저 실행해야 하는가? |
| --- | --- | --- |
| `tests/unit/` | 요청 파서·응답 생성·파일 처리 함수를 직접 호출하여 반환값 검사 | 아니요 |
| `tests/integration/` | 시험용 서버에 클라이언트로 접속하여 응답 검사 | 아니요. 테스트가 서버 시작·종료를 자동 처리 |
| `tests/fixtures/` | 정상·비정상 요청 등의 입력 데이터 제공 | 실행 대상이 아님 |

통합 테스트는 사용 가능한 임시 포트를 운영체제에서 할당받고 `listen()` 완료 후 접속하며, 시험 종료 시 서버를 정리한다. 별도의 수동 서버가 필요하지 않다.

### pytest가 테스트를 찾는 규칙

`python3 -m pytest tests/`에서 Python은 **pytest 패키지**를 실행한다. 그다음 pytest가 **tests/ 폴더**를 탐색한다. 폴더 이름이 `tests`라서 Python이 특별하게 실행하는 것은 아니다.

별도 설정이 없으면 하위 폴더를 탐색하면서 `test_*.py` 또는 `*_test.py` 파일을 찾고, 그 안에서 이름이 `test`로 시작하는 함수를 수집한다. 일반 테스트 클래스는 이름이 `Test`로 시작하고 `__init__`을 정의하지 않아야 하며, 내부의 `test`로 시작하는 메서드를 수집한다. 제외 폴더 등 설정으로 탐색 범위를 바꿀 수 있다. 이 구조의 `tests/`에는 `__init__.py`가 없어도 된다. [pytest 테스트 탐색 규칙](https://docs.pytest.org/en/stable/explanation/goodpractices.html#conventions-for-python-test-discovery)

실행 결과의 `PASSED`는 통과, `FAILED`는 실패를 뜻한다. 수집·준비 과정에서 오류가 나면 오류 내용을 확인한다. `no tests ran`은 테스트를 실행하지 않았다는 뜻으로, 검증에 통과한 결과가 아니다.

## 6. TCP Echo 실습 실행

4주차 실습은 기본 `127.0.0.1:9000`에서 실행한다.

```bash
python3 examples/tcp_echo_server.py
```

이 명령은 파일을 직접 실행하는 방식이다. Echo 서버는 받은 데이터를 되돌려주는 TCP 실습 서버이며 HTTP 서버와는 다르다. 접속 주소·포트는 실습 코드에서 정한 값을 사용하고, HTTP의 `200`·`404` 검증은 `mini_http` 서버에 수행한다.

다른 터미널에서 표준 라이브러리 클라이언트로 Echo 응답을 확인한다.

```bash
python3 - <<'PYCLIENT'
import socket

message = "안녕하세요, Echo!".encode("utf-8")
with socket.create_connection(("127.0.0.1", 9000), timeout=3) as client:
    client.sendall(message)
    client.shutdown(socket.SHUT_WR)
    received = bytearray()
    while chunk := client.recv(4096):
        received.extend(chunk)
assert received == message
print(received.decode("utf-8"))
PYCLIENT
```

## 7. 자주 만나는 실행 오류

| 메시지 또는 증상 | 확인할 내용 |
| --- | --- |
| `No module named mini_http` | 서버 패키지를 구현했는지, 프로젝트 루트에서 `PYTHONPATH=src`를 지정했는지 확인 |
| `No module named mini_http.__main__` | `src/mini_http/__main__.py` 존재 여부 확인 |
| `No module named pytest` | 현재 Python 환경에서 `python3 -m pip install pytest` 실행 여부 확인 |
| `file or directory not found: tests/` | 테스트 폴더가 구현됐는지, 현재 작업 위치가 프로젝트 루트인지 확인 |
| `Address already in use` | 설정한 포트를 다른 서버가 사용하는지 확인하고 기존 실습 서버를 종료하거나 포트 변경 |
| curl 연결 실패 | 서버 실행 여부 및 주소·포트 일치 여부 확인 |
| curl 시간 초과 | 서버 요청 수신·응답 처리 상태와 네트워크 설정 확인 |

작업을 마친 뒤 서버는 해당 터미널에서 `Ctrl+C`로 종료하고, 가상환경은 다음 명령으로 비활성화한다. `deactivate` 자체는 실행 중인 서버를 종료하지 않는다.

```bash
deactivate
```
