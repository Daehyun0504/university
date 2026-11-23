# OJT 계획 문서 표 이미지

원문 표 9개의 내용을 유지한 PNG입니다. Markdown 강조·코드 표시 기호만 제거했습니다.
시험 결과와 상태는 원문에 기록된 값이며 새로 시험하여 갱신한 값이 아닙니다.

- 00_preview.png는 전체 확인용입니다. Word에는 개별 PNG를 삽입하세요.
- 개별 PNG는 흰색 배경, 300dpi입니다. 가로세로 비율을 유지하세요.
- 권장 폭 16cm: A4 세로 문서. 권장 폭 24cm: A4 가로 문서.
- 넓은 표를 세로 페이지에 맞춰 축소하면 글자가 작아지므로 가로 페이지를 권장합니다.
- 이미지 안의 글자는 Word에서 직접 편집할 수 없습니다.

| 파일 | 표 | 픽셀 크기 | 권장 삽입 폭 |
| --- | --- | --- | --- |
| 01_requirements.png | 요구사항 및 완료 기준 | 2835 × 2079 | 24cm |
| 02_http_errors.png | 오류 응답 정책 | 1890 × 1460 | 16cm |
| 03_wbs_schedule.png | WBS 및 주차별 일정 | 2835 × 1504 | 24cm |
| 04_risks.png | 위험 및 대응 | 1890 × 1166 | 16cm |
| 05_test_results.png | 시험 계획 및 수행 결과 | 2835 × 1416 | 24cm |
| 06_lan_record.png | LAN 검증 기록 양식 | 1890 × 989 | 16cm |
| 07_defect_record.png | 결함 관리 기록 양식 | 2835 × 533 | 24cm |
| 08_change_history.png | 변경요청·결정 이력 | 1890 × 739 | 16cm |
| 09_change_request.png | 변경요청 기록 양식 | 1890 × 989 | 16cm |

재생성: `python3 docs/render_planning_tables.py` (프로젝트 루트 기준).
필요 환경: Python 3, Pillow, Noto Sans CJK 글꼴.
