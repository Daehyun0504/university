#!/usr/bin/env python3
"""Render the planning document's tables as exact-text, 300 dpi Word assets.

Requires Pillow and the Noto Sans CJK fonts installed on this workstation.
Run: python3 docs/render_planning_tables.py
"""

from pathlib import Path
import re
import zipfile

from PIL import Image, ImageDraw, ImageFont


DOCS = Path(__file__).resolve().parent
SOURCE = DOCS / 'ojt_manage_planning.md'
OUTPUT = DOCS / 'ojt_manage_planning_images'
FONT_DIR = Path('/usr/share/fonts/opentype/noto')
NAVY = '#20364B'
INK = '#243648'
GRAY = '#64748B'
TEAL = '#16857D'

# Widths are chosen for insertion at 16 cm (portrait) or 24 cm (landscape).
LAYOUTS = [
    ('01_requirements', '요구사항 및 완료 기준', 2835, [0.09, 0.19, 0.29, 0.43]),
    ('02_http_errors', '오류 응답 정책', 1890, [0.12, 0.88]),
    ('03_wbs_schedule', 'WBS 및 주차별 일정', 2835, [0.10, 0.07, 0.12, 0.29, 0.42]),
    ('04_risks', '위험 및 대응', 1890, [0.30, 0.27, 0.43]),
    ('05_test_results', '시험 계획 및 수행 결과', 2835, [0.30, 0.25, 0.45]),
    ('06_lan_record', 'LAN 검증 기록 양식', 1890, [0.40, 0.60]),
    ('07_defect_record', '결함 관리 기록 양식', 2835, [0.18, 0.25, 0.15, 0.12, 0.12, 0.18]),
    ('08_change_history', '변경요청·결정 이력', 1890, [0.24, 0.76]),
    ('09_change_request', '변경요청 기록 양식', 1890, [0.34, 0.66]),
]


def font(size, bold=False):
    name = 'NotoSansCJK-Bold.ttc' if bold else 'NotoSansCJK-Regular.ttc'
    return ImageFont.truetype(str(FONT_DIR / name), size, index=1)


def read_tables(source=SOURCE):
    tables = []
    rows = []
    for line in source.read_text(encoding='utf-8').splitlines() + ['']:
        if line.startswith('|'):
            cells = [cell.strip() for cell in line.strip().strip('|').split('|')]
            if all(re.fullmatch(r':?-+:?', cell) for cell in cells):
                continue
            rows.append([cell.replace('`', '').replace('**', '') for cell in cells])
        elif rows:
            assert all(len(row) == len(rows[0]) for row in rows)
            tables.append(rows)
            rows = []
    return tables


def wrap(text, face, max_width):
    """Wrap words where possible; split long Korean text and code names exactly."""
    lines = []
    remaining = text
    while remaining:
        if face.getlength(remaining) <= max_width:
            lines.append(remaining)
            break
        end = 1
        while end < len(remaining) and face.getlength(remaining[:end + 1]) <= max_width:
            end += 1
        # Prefer a nearby word or identifier boundary without dropping letters.
        boundaries = [i + 1 for i, char in enumerate(remaining[:end]) if char in ' _/,']
        if boundaries and boundaries[-1] >= end * 0.6:
            end = boundaries[-1]
        lines.append(remaining[:end].rstrip())
        remaining = remaining[end:].lstrip()
    assert re.sub(r'\s', '', ''.join(lines)) == re.sub(r'\s', '', text)
    assert all(face.getlength(line) <= max_width for line in lines)
    return lines or ['']


def render(rows, layout, number, *, output=OUTPUT,
           document_title='OJT 프로젝트 계획 및 수행 기록',
           source_name='ojt_manage_planning.md'):
    stem, title, width, ratios = layout
    margin = 44
    padding = 25
    table_width = width - margin * 2
    widths = [round(table_width * ratio) for ratio in ratios]
    widths[-1] = table_width - sum(widths[:-1])
    prepared = []
    for row_index, row in enumerate(rows):
        cells = []
        for col, text in enumerate(row):
            size = 38 if number == 1 and col == 3 and row_index else 40
            face = font(size, bold=row_index == 0)
            lines = wrap(text, face, widths[col] - padding * 2)
            cells.append((lines, face))
        height = max(len(lines) for lines, _ in cells) * 59 + 44
        prepared.append((height, cells))
    table_top = 187
    table_bottom = table_top + sum(height for height, _ in prepared)
    canvas = Image.new('RGB', (width, table_bottom + 81), 'white')
    draw = ImageDraw.Draw(canvas)
    draw.rectangle((margin, 36, margin + 53, 43), fill=TEAL)
    draw.text((margin, 59), title, font=font(60, True), fill=NAVY, anchor='lt')
    draw.text((margin, 140), f'{document_title}  /  표 {number:02d}',
              font=font(25), fill=GRAY, anchor='lt')
    y = table_top
    for row_index, (height, cells) in enumerate(prepared):
        background = NAVY if row_index == 0 else ('#F2F6F9' if row_index % 2 else 'white')
        draw.rectangle((margin, y, width - margin, y + height), fill=background)
        x = margin
        for col, (lines, face) in enumerate(cells):
            line_y = y + (height - len(lines) * 59) / 2 + 8
            for line in lines:
                draw.text((x + padding, line_y), line, font=face,
                          fill='white' if row_index == 0 else INK, anchor='lt')
                line_y += 59
            x += widths[col]
        if row_index:
            draw.line((margin, y + height, width - margin, y + height), fill='#D9E2EA', width=2)
        y += height
    draw.text((margin, table_bottom + 29), f'출처: {source_name}',
              font=font(23), fill=GRAY, anchor='lt')
    target = output / f'{stem}.png'
    canvas.save(target, dpi=(300, 300))
    return target, canvas.size


def main():
    tables = read_tables()
    assert len(tables) == len(LAYOUTS), 'Table count changed; update LAYOUTS.'
    OUTPUT.mkdir(exist_ok=True)
    assets = [render(rows, layout, index) for index, (rows, layout) in enumerate(zip(tables, LAYOUTS), 1)]

    tile_width, tile_height = 660, 570
    preview = Image.new('RGB', (tile_width * 3, tile_height * 3 + 120), '#E8EEF3')
    draw = ImageDraw.Draw(preview)
    draw.text((35, 33), 'OJT 수행 기록 · 표 이미지 9종', font=font(43, True), fill=NAVY, anchor='lt')
    for index, ((path, _), layout) in enumerate(zip(assets, LAYOUTS)):
        x, y = (index % 3) * tile_width + 20, (index // 3) * tile_height + 120
        draw.rounded_rectangle((x, y, x + 620, y + 545), radius=10, fill='white')
        draw.text((x + 18, y + 18), f'{index + 1:02d}  {layout[1]}', font=font(25, True), fill=NAVY, anchor='lt')
        with Image.open(path) as original:
            thumb = original.copy()
        thumb.thumbnail((595, 475), Image.Resampling.LANCZOS)
        preview.paste(thumb, (x + (620 - thumb.width) // 2, y + 61 + (475 - thumb.height) // 2))
    preview.save(OUTPUT / '00_preview.png')

    notes = [
        '# OJT 계획 문서 표 이미지', '',
        '원문 표 9개의 내용을 유지한 PNG입니다. Markdown 강조·코드 표시 기호만 제거했습니다.',
        '시험 결과와 상태는 원문에 기록된 값이며 새로 시험하여 갱신한 값이 아닙니다.', '',
        '- 00_preview.png는 전체 확인용입니다. Word에는 개별 PNG를 삽입하세요.',
        '- 개별 PNG는 흰색 배경, 300dpi입니다. 가로세로 비율을 유지하세요.',
        '- 권장 폭 16cm: A4 세로 문서. 권장 폭 24cm: A4 가로 문서.',
        '- 넓은 표를 세로 페이지에 맞춰 축소하면 글자가 작아지므로 가로 페이지를 권장합니다.',
        '- 이미지 안의 글자는 Word에서 직접 편집할 수 없습니다.', '',
        '| 파일 | 표 | 픽셀 크기 | 권장 삽입 폭 |',
        '| --- | --- | --- | --- |',
    ]
    for (path, (width, height)), layout in zip(assets, LAYOUTS):
        notes.append(f'| {path.name} | {layout[1]} | {width} × {height} | {24 if width == 2835 else 16}cm |')
        print(f'{path.name}: {width}x{height}')
    notes += ['', '재생성: `python3 docs/render_planning_tables.py` (프로젝트 루트 기준).',
              '필요 환경: Python 3, Pillow, Noto Sans CJK 글꼴.', '']
    (OUTPUT / 'README.md').write_text('\n'.join(notes), encoding='utf-8')
    with zipfile.ZipFile(DOCS / 'ojt_manage_planning_images.zip', 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(OUTPUT.iterdir()):
            if path.suffix in ('.png', '.md'):
                archive.write(path, arcname=f'{OUTPUT.name}/{path.name}')


if __name__ == '__main__':
    main()
