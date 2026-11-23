#!/usr/bin/env python3
"""Render the shortened control-process report as 300 dpi PNG tables."""

from pathlib import Path
import zipfile

from PIL import Image, ImageDraw

from render_planning_tables import NAVY, font, read_tables, render


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'ojt_manage_control_process.md'
OUTPUT = ROOT / 'ojt_manage_control_process_images'
LAYOUTS = [
    ('01_current_operations', '현행 운영 현황', 1890, [0.25, 0.75]),
    ('02_management_scope', '통합 관리 대상', 1890, [0.23, 0.48, 0.29]),
    ('03_roles', '역할 및 책임', 1890, [0.29, 0.71]),
    ('04_process', '통합 운영 프로세스', 1890, [0.19, 0.46, 0.35]),
    ('05_weekly_reporting', '주간 업무보고 체계', 1890, [0.16, 0.48, 0.36]),
    ('06_report_template', '공통 주간 보고 양식', 1890, [0.26, 0.74]),
    ('07_redmine_issue', 'Redmine 이슈 등록 항목', 1890, [0.25, 0.75]),
    ('08_metrics', '운영 점검 및 관리 지표', 1890, [0.27, 0.73]),
]


def main():
    tables = read_tables(SOURCE)
    assert len(tables) == len(LAYOUTS), '문서의 표 수와 이미지 구성을 확인하세요.'
    OUTPUT.mkdir(exist_ok=True)
    assets = []
    for index, (rows, layout) in enumerate(zip(tables, LAYOUTS), 1):
        context = '현행 운영' if index == 1 else '운영 제안'
        assets.append(render(rows, layout, index, output=OUTPUT,
                             document_title=f'회사 통합 관리 프로세스 · {context}',
                             source_name=SOURCE.name))

    preview = Image.new('RGB', (1320, 2400), '#E8EEF3')
    draw = ImageDraw.Draw(preview)
    draw.text((35, 35), '회사 통합 관리 프로세스 · 표 8종',
              font=font(40, True), fill=NAVY, anchor='lt')
    for index, ((path, _), layout) in enumerate(zip(assets, LAYOUTS)):
        x, y = (index % 2) * 660 + 20, (index // 2) * 570 + 110
        draw.rounded_rectangle((x, y, x + 620, y + 545), radius=10, fill='white')
        draw.text((x + 18, y + 18), f'{index + 1:02d}  {layout[1]}',
                  font=font(26, True), fill=NAVY, anchor='lt')
        with Image.open(path) as original:
            thumb = original.copy()
        thumb.thumbnail((595, 475), Image.Resampling.LANCZOS)
        preview.paste(thumb, (x + (620 - thumb.width) // 2,
                             y + 61 + (475 - thumb.height) // 2))
    preview.save(OUTPUT / '00_preview.png')

    notes = [
        '# 통합 관리 프로세스 표 이미지', '',
        '분량을 줄인 ojt_manage_control_process.md의 표 8개를 이미지로 만들었습니다.',
        '표의 내용을 유지하고 Markdown 서식 기호만 제거했습니다.',
        '표 1은 확인된 현행 운영이며 표 2~8은 운영 제안입니다.', '',
        '- Word에는 미리보기 대신 개별 PNG를 삽입하세요.',
        '- 모든 표는 300dpi, 권장 삽입 폭 16cm로 A4 세로 페이지에 맞췄습니다.',
        '- 가로세로 비율을 유지하세요. 이미지의 글자는 Word에서 직접 수정할 수 없습니다.',
        '- 재생성: 프로젝트 루트에서 python3 docs/render_control_process_tables.py',
        '- 필요 환경: Python 3, Pillow, Noto Sans CJK 글꼴.', '',
        '| 파일 | 표 | 픽셀 크기 |', '| --- | --- | --- |',
    ]
    for (path, (width, height)), layout in zip(assets, LAYOUTS):
        assert height < 2800, '세로 페이지에 맞도록 표 높이를 조정하세요.'
        notes.append(f'| {path.name} | {layout[1]} | {width} × {height} |')
        print(f'{path.name}: {width}x{height}')
    readme = OUTPUT / 'README.md'
    readme.write_text('\n'.join(notes) + '\n', encoding='utf-8')
    with zipfile.ZipFile(ROOT / 'ojt_manage_control_process_images.zip',
                         'w', zipfile.ZIP_DEFLATED) as archive:
        for path in [OUTPUT / '00_preview.png', *(path for path, _ in assets), readme]:
            archive.write(path, arcname=f'{OUTPUT.name}/{path.name}')


if __name__ == '__main__':
    main()
