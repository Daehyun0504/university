#!/usr/bin/env python3
"""Render the embedded-project planning example's tables for Word."""

from pathlib import Path
import zipfile

from PIL import Image, ImageDraw

from render_planning_tables import NAVY, font, read_tables, render


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'ojt_management_plan_example.md'
OUTPUT = ROOT / 'ojt_management_plan_example_images'
LAYOUTS = [
    ('01_scope_acceptance', '개발 범위 및 완료 기준', 1890, [0.25, 0.39, 0.36]),
    ('02_schedule', '작업 분해 및 일정', 1890, [0.15, 0.51, 0.34]),
    ('03_risks', '주요 위험 및 대응 계획', 1890, [0.26, 0.28, 0.46]),
]


def main():
    tables = read_tables(SOURCE)
    assert len(tables) == len(LAYOUTS)
    OUTPUT.mkdir(exist_ok=True)
    assets = []
    for index, (rows, layout) in enumerate(zip(tables, LAYOUTS), 1):
        context = '8주 예시 · 착수 조건 충족 가정' if index == 2 else '계획수립 예시'
        assets.append(render(rows, layout, index, output=OUTPUT,
                             document_title=f'임베디드 프로젝트 · {context}',
                             source_name=SOURCE.name))

    preview = Image.new('RGB', (1320, 1840), '#E8EEF3')
    draw = ImageDraw.Draw(preview)
    draw.text((40, 32), '임베디드 프로젝트 계획 · 표 3종',
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
        '# 임베디드 프로젝트 계획수립 사례 · 표 이미지', '',
        '원문 표 3개의 내용을 유지한 300dpi PNG입니다. 확정 계약이나 실적이 아닌 계획 예시입니다.',
        '64비트 기반 코드 제공 범위와 32비트 베어메탈 가정은 원문 착수 조건과 함께 확인하세요.', '',
        '- Word에는 개별 PNG를 삽입하세요. 00_preview.png는 전체 확인용입니다.',
        '- 모든 표의 권장 폭은 16cm이며 A4 세로 페이지에 맞췄습니다.',
        '- 가로세로 비율을 유지하세요. 이미지 안의 글자는 직접 편집할 수 없습니다.',
        '- 재생성: 프로젝트 루트에서 python3 docs/render_plan_example_tables.py',
        '- 필요 환경: Python 3, Pillow, Noto Sans CJK 글꼴.', '',
        '| 파일 | 표 | 픽셀 크기 |', '| --- | --- | --- |',
    ]
    for (path, (width, height)), layout in zip(assets, LAYOUTS):
        assert height < 2800
        notes.append(f'| {path.name} | {layout[1]} | {width} × {height} |')
        print(f'{path.name}: {width}x{height}')
    readme = OUTPUT / 'README.md'
    readme.write_text('\n'.join(notes) + '\n', encoding='utf-8')
    with zipfile.ZipFile(ROOT / 'ojt_management_plan_example_images.zip',
                         'w', zipfile.ZIP_DEFLATED) as archive:
        for path in [OUTPUT / '00_preview.png', *(path for path, _ in assets), readme]:
            archive.write(path, arcname=f'{OUTPUT.name}/{path.name}')


if __name__ == '__main__':
    main()
