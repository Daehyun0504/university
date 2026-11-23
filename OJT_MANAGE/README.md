# OJT · 미니 HTTP 서버

Python 표준 라이브러리의 `socket`으로 만든 학습용 정적 HTTP 서버다. Flask·Django·FastAPI와 `http.server`를 사용하지 않고 TCP 연결, HTTP 요청 파싱, 응답 생성과 파일 제공을 직접 구현한다. Python 3.10 이상에서 사용하며 자동 시험에만 pytest를 사용한다.

## 바로 실행하기

서버 실행에는 별도 패키지 설치가 필요 없다. 저장소 루트에서 실행한다.

```bash
cd /home/yoondh/work/OJT_MANAGE
PYTHONPATH=src python3 -m mini_http
```

다른 터미널에서 접속한다. 브라우저에서도 같은 주소를 열 수 있다.

```bash
curl -i --max-time 5 http://127.0.0.1:8000/
curl -i --max-time 5 http://127.0.0.1:8000/not-found
```

기본 경로는 한글 HTML과 `200`, 없는 경로는 안내 페이지와 `404`를 반환한다. 서버는 `Ctrl+C`로 종료한다.

## 설정

```bash
# 다른 LAN 기기에서 접속할 수 있도록 실행
PYTHONPATH=src python3 -m mini_http --host 0.0.0.0 --port 8000

# 다른 문서 루트와 포트 사용
PYTHONPATH=src python3 -m mini_http --root ./www --port 8080

# 모든 옵션 확인
PYTHONPATH=src python3 -m mini_http --help
```

| 옵션 | 기본값 | 의미 |
| --- | --- | --- |
| `--host` | `127.0.0.1` | IPv4 바인딩 주소 |
| `--port` | `8000` | TCP 포트. `0`은 운영체제가 사용 가능한 포트 할당 |
| `--root` | 프로젝트의 `www/` | 파일 제공 디렉터리. 기본 경로는 실행 위치와 무관 |
| `--timeout` | `5` | 전체 요청 헤더 수신 제한 시간(초). 응답 전송에도 같은 타임아웃 적용 |
| `--max-header-bytes` | `16384` | 요청 라인·헤더·종료 구분자를 포함한 바이트 상한 |
| `--max-file-bytes` | `8388608` | 제공 가능한 정적 파일 크기 상한 |

LAN 클라이언트는 `http://서버의실제LAN_IP:8000/`에 접속한다. 바인딩 주소 `0.0.0.0`을 접속 주소로 사용하지 않는다.

## 자동 테스트

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -e '.[test]'
PYTHONPATH=src python3 -m pytest tests/ -v
```

`venv` 생성 시 `ensurepip` 오류가 발생하는 Ubuntu/Debian 환경은 `python3-venv` 패키지를 설치한 뒤 다시 생성한다. 이 작업 환경에서는 `.venv`와 pytest를 준비해 두었으므로 바로 `source .venv/bin/activate`부터 실행할 수 있다.

```bash
# 내부 함수만 검증
python3 -m pytest tests/unit/ -v

# 실제 소켓 연결 검증: 시험용 서버 시작·종료를 자동 처리
python3 -m pytest tests/integration/ -v

# 수집할 테스트 확인
python3 -m pytest --collect-only -q
```

pytest의 탐색 경로와 `src` 경로는 `pyproject.toml`에도 설정되어 있다. 통합 시험은 임시 포트를 사용하므로 수동 서버를 미리 실행할 필요가 없다.

## 구현 범위

- `GET` 정적 파일 제공, `/ → www/index.html`, 쿼리 문자열을 제외한 파일 경로 매핑
- UTF-8 텍스트, MIME 타입, 바이너리 본문과 바이트 기준 `Content-Length`
- HTTP/1.0 및 HTTP/1.1 형식의 요청, HTTP/1.1의 필수 Host 확인
- 연결당 첫 요청 하나를 처리하고 항상 `Connection: close`로 종료
- 경로 탈출·외부 심볼릭 링크 차단, 수신 시간·헤더 크기·파일 크기 제한
- `400/403/404/405/408/417/431/500/501/505` 오류 응답

학습 범위는 IPv4 순차 처리 서버다. Keep-Alive, 동시 요청 처리, TLS, 동적 페이지, 요청 본문·업로드, 압축 전송, 전체 HTTP 표준 준수는 포함하지 않는다. 중복 헤더는 모두 `400`으로 거부한다. GET 이외의 알려진 메서드는 `405`와 `Allow: GET`, 모르는 메서드는 `501`을 반환한다. `HEAD`의 오류 응답은 본문을 보내지 않는다. 제공하는 텍스트 파일은 UTF-8로 작성하며 문서 루트는 신뢰할 수 있는 로컬 실습 파일로 관리한다.

## TCP Echo 실습

```bash
python3 examples/tcp_echo_server.py --port 9000
```

HTTP 서버와 별도의 4주차 실습이다. 클라이언트 예시는 [서버 명령어 문서](./docs/ojt_manage_server_command.md)에 있다.

## 문서

- [수업 계획서](./ojt_manage_plan.md)
- [디렉터리 구조와 모듈 역할](./ojt_manage_dir_diagram.md)
- [15주 진행 흐름](./ojt_manage_flow_diagram.md)
- [요구사항·WBS·시험·변경·종료 기록](./docs/ojt_manage_planning.md)
- [서버 실행 및 테스트 명령어](./docs/ojt_manage_server_command.md)
- [Linux 클라이언트 접속 명령어](./docs/ojt_manage_client_command.md)

현재 검증 결과와 LAN·스마트폰 등 미검증 항목은 통합 계획 문서에 구분해 기록한다. 15주 수업의 승인·발표·종료가 완료되었다는 의미는 아니다.
