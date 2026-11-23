#!/usr/bin/env python3
"""Render the implemented raw TCP HTTP server report tables for Word."""

from pathlib import Path
import zipfile

from PIL import Image, ImageDraw

from render_planning_tables import NAVY, font, read_tables, render


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'ojt_manage_raw_server_report.md'
OUTPUT = ROOT / 'ojt_manage_raw_server_report_images'
LAYOUTS = [
    ('01_project_management', '프로젝트 관리 적용 및 기록', 1890, [0.18, 0.43, 0.39]),
    ('02_modules', '프로그램 구성 및 역할', 1890, [0.19, 0.43, 0.38]),
    ('03_learning', '네트워크 학습의 핵심 내용', 1890, [0.22, 0.35, 0.43]),
    ('04_verification', '검증 방법 및 확인 결과', 1890, [0.21, 0.40, 0.39]),
]


def main():
    tables = read_tables(SOURCE)
    assert len(tables) == len(LAYOUTS)
    OUTPUT.mkdir(exist_ok=True)
    assets = []
    for index, (rows, layout) in enumerate(zip(tables, LAYOUTS), 1):
        context = '코드·시험·기존 기록 기준 · 2026-09-10'
        assets.append(render(rows, layout, index, output=OUTPUT,
                             document_title=f'Raw_Server · {context}',
                             source_name=SOURCE.name))

    preview = Image.new('RGB', (1320, 110 + len(assets) * 640), '#E8EEF3')
    draw = ImageDraw.Draw(preview)
    draw.text((40, 32), 'Raw_Server 프로젝트 보고서 · 표 4종',
              font=font(44, True), fill=NAVY, anchor='lt')
    for index, ((path, _), layout) in enumerate(zip(assets, LAYOUTS)):
        y = 110 + index * 640
        draw.rounded_rectangle((20, y, 1300, y + 620), radius=10, fill='white')
        draw.text((45, y + 17), f'{index + 1:02d}  {layout[1]}',
                  font=font(30, True), fill=NAVY, anchor='lt')
        with Image.open(path) as original:
            thumb = original.copy()
        thumb.thumbnail((1230, 540), Image.Resampling.LANCZOS)
        preview.paste(thumb, ((1320 - thumb.width) // 2,
                             y + 68 + (540 - thumb.height) // 2))
    preview.save(OUTPUT / '00_preview.png')

    notes = [
        '# Raw_Server 프로젝트 보고서 · 표 이미지', '',
        '원문 표 4개의 내용을 유지한 300dpi PNG입니다. 프로젝트 관리 기록, 현재 소스, 이번 자동 시험과 기존 문서의 기록을 구분하여 표시했습니다.',
        '자동 시험 77개 통과(2.69초)는 2026-09-10 실행 결과입니다. LAN·화면·패킷 관찰은 미검증입니다.', '',
        '- Word에는 개별 PNG를 삽입하세요. 00_preview.png는 전체 확인용입니다.',
        '- 모든 표의 권장 폭은 16cm이며 A4 세로 페이지에 맞췄습니다.',
        '- 가로세로 비율을 유지하세요. 이미지 안의 글자는 직접 편집할 수 없습니다.',
        '- 재생성: 프로젝트 루트에서 python3 docs/render_raw_server_tables.py',
        '- 필요 환경: Python 3, Pillow, Noto Sans CJK 글꼴.', '',
        '| 파일 | 표 | 픽셀 크기 |', '| --- | --- | --- |',
    ]
    for (path, (width, height)), layout in zip(assets, LAYOUTS):
        assert height < 2800
        notes.append(f'| {path.name} | {layout[1]} | {width} × {height} |')
        print(f'{path.name}: {width}x{height}')
    readme = OUTPUT / 'README.md'
    readme.write_text('\n'.join(notes) + '\n', encoding='utf-8')
    with zipfile.ZipFile(ROOT / 'ojt_manage_raw_server_report_images.zip',
                         'w', zipfile.ZIP_DEFLATED) as archive:
        for path in [OUTPUT / '00_preview.png', *(path for path, _ in assets), readme]:
            archive.write(path, arcname=f'{OUTPUT.name}/{path.name}')


if __name__ == '__main__':
    main()
