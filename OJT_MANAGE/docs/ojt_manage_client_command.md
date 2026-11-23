# Linux 클라이언트 접속 명령어

관련 문서: [서버 실행 및 테스트 명령어](./ojt_manage_server_command.md) · [디렉터리 구조도](../ojt_manage_dir_diagram.md)

이 문서는 Linux 터미널에서 HTTP 클라이언트로 접속하는 방법을 정리한다. **서버를 실행한 뒤** 다른 터미널에서 사용한다. 예시 주소는 `http://127.0.0.1:8000/`이며 실제 설정에 맞게 바꾼다. 명령어는 프로젝트 폴더 밖에서도 실행할 수 있고 Python 가상환경이나 GUI는 필요하지 않다.

## 1. curl 준비

```bash
curl --version
```

`command not found`가 나오면 사용하는 배포판의 패키지 관리자로 설치한다. Ubuntu/Debian에서는 다음 명령을 사용한다.

```bash
sudo apt update
sudo apt install curl
```

## 2. 로컬 서버에 접속

서버를 실행한 컴퓨터의 **다른 터미널**에서 실행한다.

접속 스크립트는 기본 주소의 응답 헤더와 HTML 본문을 출력하고 종료한다. 선택적으로 접속 URL 하나를 지정할 수 있다. 요청 제한 시간은 5초이며 프록시를 거치지 않고 직접 접속한다.

```bash
/home/yoondh/work/OJT_MANAGE/docs/ojt_manage_run_client
/home/yoondh/work/OJT_MANAGE/docs/ojt_manage_run_client http://127.0.0.1:8080/
/home/yoondh/work/OJT_MANAGE/docs/ojt_manage_run_client --help
```

curl 명령을 직접 사용하는 방법은 다음과 같다.

```bash
# 응답 본문(HTML) 확인
curl --max-time 5 http://127.0.0.1:8000/

# 상태 줄·응답 헤더·본문 확인
curl -i --max-time 5 http://127.0.0.1:8000/

# 없는 경로에 대한 404 응답 확인
curl -i --max-time 5 http://127.0.0.1:8000/not-found
```

기본 경로는 `200`과 `www/index.html`의 내용을, 없는 경로는 `404`를 반환한다. curl은 HTML 소스를 출력하며 브라우저처럼 화면을 그리지는 않는다.

## 3. 요청과 응답을 자세히 확인

```bash
# 연결 정보·요청 헤더·응답 헤더와 본문 확인
curl -v --max-time 5 http://127.0.0.1:8000/

# GET 요청의 응답 헤더만 표시하고 본문은 버리기
curl -sS -D - -o /dev/null --max-time 5 http://127.0.0.1:8000/

# HTTP/1.1 요청과 연결 종료 요청 헤더를 명시
curl --http1.1 -H 'Connection: close' -i --max-time 5 http://127.0.0.1:8000/
```

위 헤더 확인 명령은 `GET`을 사용한다. `curl -I`는 `HEAD` 요청이므로, 기본 구현 범위가 `GET`인 이 프로젝트에서는 같은 시험으로 취급하지 않는다.

## 4. 상태 코드만 확인

```bash
curl -sS -o /dev/null -w '%{http_code}\n' --max-time 5 http://127.0.0.1:8000/
curl -sS -o /dev/null -w '%{http_code}\n' --max-time 5 http://127.0.0.1:8000/not-found
```

기대 출력은 각각 `200`, `404`다. 이 명령은 상태를 표시하며 기대값과의 일치를 자동 판정하지 않는다. curl은 기본적으로 HTTP `404`만으로 실행 실패를 반환하지 않는다. `000`이 표시되면 실제 HTTP 상태 코드가 아니라 응답 코드를 받지 못한 상황이므로 함께 출력된 오류를 확인한다.

## 5. 응답 HTML을 파일로 저장

```bash
curl -sS --max-time 5 -o ojt_index_response.html http://127.0.0.1:8000/
cat ojt_index_response.html
```

현재 작업 폴더에 저장하며, 같은 이름의 파일이 있으면 덮어쓴다. 응답 상태가 오류이면 오류 본문이 저장될 수도 있으므로 상태 코드도 확인한다.

## 6. 다른 Linux 컴퓨터에서 LAN 접속

서버가 LAN 연결을 받도록 설정되어 있어야 한다. 서버 실행 옵션은 `--host 0.0.0.0 --port 8000`이다. 서버 컴퓨터의 LAN IP가 `192.168.0.10`이라면 같은 네트워크의 클라이언트에서 다음을 실행한다.

```bash
curl -i --max-time 5 http://192.168.0.10:8000/
```

`192.168.0.10`은 예시이므로 실제 서버 IP로 바꾼다. 클라이언트에서 `127.0.0.1`은 클라이언트 자신을 가리키고, `0.0.0.0`은 서버의 수신 설정값이므로 접속 주소로 쓰지 않는다.

## 7. 옵션 및 접속 문제 확인

| 옵션 | 역할 |
| --- | --- |
| `-i` | 응답 헤더와 본문 표시 |
| `-v` | 연결 과정과 요청·응답 헤더 등 상세 정보 표시 |
| `-sS` | 진행 표시를 숨기고 오류 메시지는 표시 |
| `-D -` | 응답 헤더를 표준 출력으로 표시 |
| `-o` | 응답 본문의 저장 위치 지정 |
| `-w` | 요청 후 상태 코드 등 지정한 정보 출력 |
| `--max-time 5` | 전체 요청 시간을 최대 5초로 제한 |

옵션의 세부 동작은 [curl 공식 매뉴얼](https://curl.se/docs/manpage.html)을 기준으로 한다.

| 증상 | 확인할 내용 |
| --- | --- |
| 연결 거부 또는 연결 실패 | 서버 실행 여부, IP·포트 일치 여부, 바인딩 주소 |
| 시간 초과 | 서버 응답 처리, 방화벽, 네트워크 연결 |
| 로컬 성공·LAN 실패 | 서버의 LAN 수신 설정, 방화벽 TCP 포트 허용, 공유기 기기 간 통신 제한 |
| 프록시 관련 응답 | 로컬 실습 서버에 직접 접속하려면 아래 명령 사용 |

```bash
curl --noproxy '*' -i --max-time 5 http://127.0.0.1:8000/
```

`--noproxy '*'`는 이번 요청에서 프록시를 거치지 않도록 한다.
