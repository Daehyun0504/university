#!/usr/bin/env python3
"""Render the fictional embedded-project control report tables for Word."""

from pathlib import Path
import zipfile

from PIL import Image, ImageDraw

from render_planning_tables import NAVY, font, read_tables, render


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'ojt_manage_press_report.md'
OUTPUT = ROOT / 'ojt_manage_press_report_images'
LAYOUTS = [
    ('01_control_baseline', '기준선과 통제 기준', 1890, [0.17, 0.41, 0.42]),
    ('02_schedule_options', '일정 지연 대응 대안', 1890, [0.23, 0.44, 0.33]),
    ('03_control_results', '통제 결과 및 확인 기록', 1890, [0.18, 0.41, 0.41]),
]


def main():
    tables = read_tables(SOURCE)
    assert len(tables) == len(LAYOUTS)
    OUTPUT.mkdir(exist_ok=True)
    assets = []
    for index, (rows, layout) in enumerate(zip(tables, LAYOUTS), 1):
        context = '가상 통제 사례 · 실제 실적 아님'
        assets.append(render(rows, layout, index, output=OUTPUT,
                             document_title=f'임베디드 프로젝트 · {context}',
                             source_name=SOURCE.name))

    preview = Image.new('RGB', (1320, 1840), '#E8EEF3')
    draw = ImageDraw.Draw(preview)
    draw.text((40, 32), '임베디드 프로젝트 통제 사례 · 표 3종',
              font=font(44, True), fill=NAVY, anchor='lt')
    for index, ((path, _), layout) in enumerate(zip(assets, LAYOUTS)):
        y = 110 + index * 570
        draw.rounded_rectangle((20, y, 1300, y + 550), radius=10, fill='white')
        draw.text((45, y + 17), f'{index + 1:02d}  {layout[1]}',
                  font=font(30, True), fill=NAVY, anchor='lt')
        with Image.open(path) as original:
            thumb = original.copy()
        thumb.thumbnail((1230, 470), Image.Resampling.LANCZOS)
        preview.paste(thumb, ((1320 - thumb.width) // 2,
                             y + 68 + (470 - thumb.height) // 2))
    preview.save(OUTPUT / '00_preview.png')

    notes = [
        '# 임베디드 프로젝트 통제 사례 · 표 이미지', '',
        '원문 표 3개의 내용을 유지한 300dpi PNG입니다. 진행 경과와 결과는 가상 사례이며 실제 실적이 아닙니다.',
        '실제 보고에 사용할 때는 기준선·결정·시험·인수 기록에 맞게 원문과 이미지를 갱신하세요.', '',
        '- Word에는 개별 PNG를 삽입하세요. 00_preview.png는 전체 확인용입니다.',
        '- 모든 표의 권장 폭은 16cm이며 A4 세로 페이지에 맞췄습니다.',
        '- 가로세로 비율을 유지하세요. 이미지 안의 글자는 직접 편집할 수 없습니다.',
        '- 재생성: 프로젝트 루트에서 python3 docs/render_press_report_tables.py',
        '- 필요 환경: Python 3, Pillow, Noto Sans CJK 글꼴.', '',
        '| 파일 | 표 | 픽셀 크기 |', '| --- | --- | --- |',
    ]
    for (path, (width, height)), layout in zip(assets, LAYOUTS):
        assert height < 2800
        notes.append(f'| {path.name} | {layout[1]} | {width} × {height} |')
        print(f'{path.name}: {width}x{height}')
    readme = OUTPUT / 'README.md'
    readme.write_text('\n'.join(notes) + '\n', encoding='utf-8')
    with zipfile.ZipFile(ROOT / 'ojt_manage_press_report_images.zip',
                         'w', zipfile.ZIP_DEFLATED) as archive:
        for path in [OUTPUT / '00_preview.png', *(path for path, _ in assets), readme]:
            archive.write(path, arcname=f'{OUTPUT.name}/{path.name}')


if __name__ == '__main__':
    main()
