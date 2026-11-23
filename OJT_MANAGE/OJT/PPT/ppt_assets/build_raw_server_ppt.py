"""Build editable, diagram-led slides from ppt_manuscript.md.

Dependency: python-pptx. Run this file with a Python environment containing it.
"""
from pathlib import Path
import math
import re

from pptx import Presentation
from pptx.util import Inches, Pt
from pptx.dml.color import RGBColor
from pptx.enum.shapes import MSO_SHAPE, MSO_CONNECTOR
from pptx.enum.text import PP_ALIGN, MSO_ANCHOR
from pptx.oxml.xmlchemy import OxmlElement

OUT = Path(__file__).resolve().parent.parent
MANUSCRIPT = (OUT / 'ppt_manuscript.md').read_text(encoding='utf-8')
prs = Presentation()
prs.slide_width = Inches(13.333333)
prs.slide_height = Inches(7.5)
prs.core_properties.title = 'Raw Server | 네트워크와 프로젝트 관리'
prs.core_properties.author = '윤대현'
prs.core_properties.subject = 'OJT 프로젝트 관리 실무'
prs.core_properties.keywords = 'Raw Server, Socket, TCP, OSI, 프로젝트 관리'
FONT = 'Noto Sans CJK KR'
MONO = 'DejaVu Sans Mono'
NAVY = '10283F'
DARK = '0B1B2A'
INK = '19354B'
MUTED = '617586'
TEAL = '007F83'
CYAN = '47D6D0'
BLUE = '3478C0'
ORANGE = 'E59743'
BG = 'F4F7FA'
WHITE = 'FFFFFF'
PALE = 'E3F4F3'
LINE = 'CEDBE4'
SECTIONS = {
    1: '기본 정보', 2: 'Raw Server 제작 이유', 3: '네트워크 7계층',
    4: 'TCP 프로그래밍', 5: '디렉터리 다이어그램', 6: '플로우 다이어그램',
    7: '프로젝트 관리', 8: '감사합니다',
}
sections = {}
for part in re.split(r'^## (?=\d\. )', MANUSCRIPT, flags=re.M)[1:]:
    sections[int(part[0])] = part
seen_notes = set()


def color(value):
    return RGBColor.from_string(value)


def shape(slide, x, y, w, h, fill=WHITE, stroke=None, kind=MSO_SHAPE.RECTANGLE, radius=False):
    obj = slide.shapes.add_shape(kind, Inches(x), Inches(y), Inches(w), Inches(h))
    obj._element.spPr.append(OxmlElement('a:effectLst'))
    obj.fill.solid()
    obj.fill.fore_color.rgb = color(fill)
    if stroke:
        obj.line.color.rgb = color(stroke)
        obj.line.width = Pt(1.2)
    else:
        obj.line.fill.background()
    if kind == MSO_SHAPE.ROUNDED_RECTANGLE:
        obj.adjustments[0] = 0.12
    return obj


def box(slide, x, y, w, h, fill=WHITE, stroke=None):
    return shape(slide, x, y, w, h, fill, stroke, MSO_SHAPE.ROUNDED_RECTANGLE)


def text(slide, x, y, w, h, value, size=24, fill=INK, bold=False,
         align=PP_ALIGN.LEFT, font=FONT, valign=MSO_ANCHOR.MIDDLE):
    obj = slide.shapes.add_textbox(Inches(x), Inches(y), Inches(w), Inches(h))
    tf = obj.text_frame
    tf.word_wrap = True
    tf.margin_left = tf.margin_right = Inches(0.01)
    tf.margin_top = tf.margin_bottom = 0
    tf.vertical_anchor = valign
    for i, line_value in enumerate(value.split('\n')):
        p = tf.paragraphs[0] if i == 0 else tf.add_paragraph()
        p.alignment = align
        p.space_before = Pt(0)
        p.space_after = Pt(3)
        r = p.add_run()
        r.text = line_value
        r.font.name = font
        r.font.size = Pt(size)
        r.font.bold = bold
        r.font.color.rgb = color(fill)
        # Explicit East Asian typeface avoids theme font substitutions.
        ea = OxmlElement('a:ea')
        ea.set('typeface', font)
        r._r.get_or_add_rPr().append(ea)
    return obj


def line(slide, x1, y1, x2, y2, fill=LINE, width=2, arrow=False):
    obj = slide.shapes.add_connector(MSO_CONNECTOR.STRAIGHT,
                                    Inches(x1), Inches(y1), Inches(x2), Inches(y2))
    obj._element.spPr.append(OxmlElement('a:effectLst'))
    obj.line.color.rgb = color(fill)
    obj.line.width = Pt(width)
    if arrow:
        end = OxmlElement('a:tailEnd')
        end.set('type', 'triangle')
        obj.line._get_or_add_ln().append(end)
    return obj


def circle(slide, x, y, d, fill=TEAL, stroke=None):
    return shape(slide, x, y, d, d, fill, stroke, MSO_SHAPE.OVAL)


def pill(slide, x, y, w, label, fill=PALE, fg=TEAL, size=16):
    box(slide, x, y, w, .4, fill)
    text(slide, x+.08, y, w-.16, .4, label, size, fg, True, PP_ALIGN.CENTER)


def tag(slide, x, y, label, fill=TEAL):
    circle(slide, x, y, .43, fill)
    text(slide, x, y, .43, .43, label, 15, WHITE, True, PP_ALIGN.CENTER)


def note(slide, section, extra=''):
    part = sections.get(section, '')
    paragraphs = []
    if extra:
        paragraphs.append(extra)
    if section not in seen_notes and '### 발표 대본' in part:
        narration = part.split('### 발표 대본', 1)[1].strip()
        paragraphs.append('원고의 발표 대본\n' + narration)
        seen_notes.add(section)
    sources = list(dict.fromkeys(re.findall(r'https?://[^)\s]+', part)))
    if sources:
        paragraphs.append('원고 참고 출처\n' + '\n'.join(sources))
    paragraphs.append('원고: ppt_manuscript.md / ' + SECTIONS[section])
    slide.notes_slide.notes_text_frame.text = '\n\n'.join(paragraphs)


def new(section, title, subtitle='', dark=False):
    slide = prs.slides.add_slide(prs.slide_layouts[6])
    slide.background.fill.solid()
    slide.background.fill.fore_color.rgb = color(DARK if dark else BG)
    fg = WHITE if dark else INK
    text(slide, .55, .25, 11, .35, f'{section:02d}  /  {SECTIONS[section]}',
         13, CYAN if dark else TEAL, True)
    text(slide, .55, .9, 12.2, .68, title, 34, fg, True)
    if subtitle:
        text(slide, .58, 1.63, 12, .42, subtitle, 17, 'AEC0CE' if dark else MUTED)
    line(slide, .55, 7.06, 12.78, 7.06, '284154' if dark else LINE, 1)
    text(slide, .58, 7.14, 8.9, .2, 'RAW SERVER   /   OJT 프로젝트 관리 실무   /   윤대현',
         9, 'AEC0CE' if dark else MUTED)
    text(slide, 11.8, 7.11, .98, .28, f'{len(prs.slides):02d} / 17',
         11, CYAN if dark else TEAL, True, PP_ALIGN.RIGHT)
    return slide


def node(slide, x, y, w, h, label, small='', fill=WHITE, accent=TEAL, size=23):
    box(slide, x, y, w, h, fill)
    shape(slide, x, y+.14, .045, h-.28, accent)
    if small:
        text(slide, x+.18, y+.14, w-.36, .55, label, size, INK, True)
        text(slide, x+.18, y+.73, w-.36, h-.8, small, 17, MUTED)
    else:
        text(slide, x+.12, y+.1, w-.24, h-.2, label, size, INK, True, PP_ALIGN.CENTER)


def server_icon(slide, x, y, s=1, fg=CYAN, bg=NAVY):
    for i in range(3):
        box(slide, x, y+i*.4*s, 1.45*s, .3*s, bg, fg)
        circle(slide, x+.12*s, y+(i*.4+.09)*s, .09*s, fg)
        line(slide, x+.95*s, y+(i*.4+.15)*s, x+1.27*s, y+(i*.4+.15)*s, fg, 2)


def laptop(slide, x, y, s=1, fg=TEAL, bg=WHITE):
    box(slide, x, y, 1.5*s, 1*s, bg, fg)
    line(slide, x-.14*s, y+1.12*s, x+1.64*s, y+1.12*s, fg, 4)
    text(slide, x+.14*s, y+.14*s, 1.2*s, .65*s, '>_', int(25*s), fg, True, PP_ALIGN.CENTER, MONO)


def folder(slide, x, y, s=1, fill=TEAL):
    box(slide, x, y, .65*s, .3*s, fill)
    box(slide, x, y+.18*s, 1.25*s, .78*s, fill)


def network(slide, x, y, w, h, dark=True):
    points = [(0.1,.43),(.35,.16),(.38,.7),(.65,.42),(.94,.16),(.9,.77)]
    edges = [(0,1),(0,2),(1,3),(2,3),(3,4),(3,5),(1,4),(2,5)]
    for a,b in edges:
        ax,ay=points[a]; bx,by=points[b]
        line(slide, x+ax*w, y+ay*h, x+bx*w, y+by*h,
             '2D5967' if dark else 'A7D9D5', 2)
    for i,(px,py) in enumerate(points):
        circle(slide, x+px*w-.1, y+py*h-.1, .2, CYAN if dark else TEAL)
        if i in (1,3,5):
            circle(slide, x+px*w-.04, y+py*h-.04, .08, WHITE)


# 01 — Cover
s = new(1, '', dark=True)
pill(s,.6,1.15,2.0,'NETWORK × PM',fill='173849',fg=CYAN,size=15)
text(s,.58,1.93,7.4,1.15,'Raw Server',66,WHITE,True)
text(s,.62,3.27,7.5,.6,'직접 만드는 네트워크',30,WHITE,True)
text(s,.62,3.96,7.5,.52,'직접 경험하는 프로젝트 관리',25,'B8CAD6')
shape(s,.63,4.83,.9,.055,CYAN)
text(s,.62,5.24,7.4,.42,'윤대현  ·  2024531046',23,WHITE,True)
text(s,.62,5.83,7.5,.4,'한국공학대학교 컴퓨터전자공학과',19,'B8CAD6')
network(s,8.0,1.15,4.6,4.9)
server_icon(s,9.35,3.0,1.35)
note(s,1)

# 02 — Agenda
s = new(1,'오늘의 흐름','WHY → THEORY → PRACTICE → MANAGEMENT')
agenda=[('01','기본 정보','소개'),('02','제작 이유','동기'),('03','네트워크 7계층','이론'),
        ('04','TCP 프로그래밍','이론'),('05','디렉터리 구조','실습'),('06','실행·시험 흐름','실습'),
        ('07','프로젝트 관리','관리'),('08','감사합니다','마무리')]
for i,(n,title,category) in enumerate(agenda):
    x=.6+(i%4)*3.18; y=2.5+(i//4)*1.88
    box(s,x,y,2.97,1.56,WHITE)
    text(s,x+.17,y+.15,.64,.51,n,28,TEAL,True)
    text(s,x+.18,y+.8,2.64,.48,title,21,INK,True)
    text(s,x+.95,y+.2,1.72,.32,category,13,MUTED,False,PP_ALIGN.RIGHT)
note(s,1,'8개 발표 항목을 유지하되, 이론과 실습의 도식을 여러 장으로 나누었습니다.')

# 03 — Why
s = new(2,'하나의 프로젝트, 두 가지 배움','4주차 네트워크 학습을 실제 결과물로 연결')
box(s,.7,2.37,5.4,3.25,WHITE)
box(s,7.23,2.37,5.4,3.25,WHITE)
network(s,1.05,2.52,2.0,1.6,False)
text(s,3.02,2.85,2.83,.66,'네트워크',29,INK,True)
text(s,1.05,4.22,4.75,.82,'Socket · HTTP\n수신 → 해석 → 응답',22,TEAL,True)
for i in range(3):
    box(s,7.66,2.76+i*.42,.25,.25,PALE)
    text(s,7.66,2.73+i*.42,.25,.3,'✓',15,TEAL,True,PP_ALIGN.CENTER)
    line(s,8.05,2.9+i*.42,8.76,2.9+i*.42,TEAL,3)
text(s,9.04,2.85,3.1,.66,'프로젝트 관리',28,INK,True)
text(s,7.65,4.22,4.6,.82,'범위 · 작업 · 검증\n계획 → 수행 → 통제',22,TEAL,True)
circle(s,6.17,3.44,.99,TEAL)
text(s,6.17,3.44,.99,.99,'+',40,WHITE,False,PP_ALIGN.CENTER)
pill(s,3.54,6.13,6.25,'프레임워크 없이, 처리 과정을 직접 구현',fill=NAVY,fg=WHITE,size=20)
note(s,2)

# 04 — OSI
s = new(3,'7계층, 역할을 나누어 이해하기','OSI는 참조 모델 · 실제 구현의 모듈 수와 일치하지 않음')
layers=[('7','응용','HTTP',TEAL),('6','표현','인코딩 · 표현','278F98'),
        ('5','세션','대화 · 세션','479DA7'),('4','전송','TCP / UDP · Port',BLUE),
        ('3','네트워크','IP · Routing','4E729B'),('2','데이터 링크','MAC · Frame','596F86'),
        ('1','물리','PHY · Signal','6D7B89')]
for i,(n,name,key,c) in enumerate(layers):
    y=2.28+i*.58
    box(s,.72,y,7.35,.5,c)
    text(s,.85,y,.48,.5,n,20,WHITE,True,PP_ALIGN.CENTER)
    text(s,1.55,y,2.3,.5,name,20,WHITE,True)
    text(s,4.15,y,3.48,.5,key,19,WHITE)
box(s,8.56,2.3,4.05,1.65,PALE)
text(s,8.84,2.5,3.5,.4,'직접 구현',19,TEAL,True)
text(s,8.84,3.03,3.5,.5,'HTTP 요청 · 응답',26,INK,True)
box(s,8.56,4.22,4.05,1.68,WHITE)
text(s,8.84,4.42,3.5,.4,'운영체제 활용',19,MUTED,True)
text(s,8.84,4.98,3.5,.52,'TCP/IP + Socket API',21,INK,True)
text(s,.75,6.56,11.7,.27,'Ethernet: 유니캐스트 · 멀티캐스트 · 브로드캐스트',15,MUTED)
note(s,3,'물리 계층을 PHY 하드웨어와 완전히 동일시하지 않습니다. MAC 통신은 1:1만 있는 것이 아닙니다.')

# 05 — Encapsulation
s = new(3,'캡슐화: 데이터에 정보를 더하다','상위 데이터를 하위 계층의 데이터 영역에 담는 구조')
rows=[('HTTP', [('데이터',7.5,TEAL)]),
      ('TCP',[('TCP',1.25,BLUE),('데이터 일부',6.25,TEAL)]),
      ('IP',[('IP',1.1,'536D91'),('TCP',1.25,BLUE),('데이터 일부',5.15,TEAL)]),
      ('Ethernet',[('MAC',1.18,'687B8C'),('IP',1.05,'536D91'),('TCP',1.16,BLUE),('데이터 일부',3.25,TEAL),('FCS',.86,'687B8C')])]
for i,(name,segments) in enumerate(rows):
    y=2.38+i*.86
    text(s,.75,y,2.13,.59,name,23,INK,True)
    x=3.05
    for label,w,c in segments:
        shape(s,x,y,w,.59,c)
        text(s,x+.04,y,w-.08,.59,label,19,WHITE,True,PP_ALIGN.CENTER)
        x+=w
line(s,11.35,2.52,11.35,5.58,TEAL,4,True)
text(s,11.56,3.38,1.17,1.1,'감싸기\n송신',19,TEAL,True,PP_ALIGN.CENTER)
box(s,.75,6.08,11.83,.61,NAVY)
text(s,.95,6.12,11.4,.49,'HTTP 요청 1개  ≠  TCP 세그먼트 1개',23,WHITE,True,PP_ALIGN.CENTER)
note(s,3,'프레임 그림은 일반적인 Ethernet 기반 TCP/IP를 단순화했습니다. 수신 측에서는 반대로 헤더를 해석합니다. PHY에서는 실제 신호로 전달되며, 127.0.0.1 로컬 시험은 물리 네트워크를 거치지 않습니다.')

# 06 — MTU
s = new(3,'1500과 4096은 서로 다른 기준','일반적인 Ethernet IP MTU 예시')
text(s,.8,2.32,4.5,1.04,'1500',68,TEAL,True)
text(s,4.7,2.66,1.35,.54,'bytes',24,MUTED)
text(s,7.2,2.55,5.2,.82,'20 + 20 + 1460',35,INK,True,PP_ALIGN.RIGHT)
parts=[('IPv4 헤더','20',1.9,'536D91'),('TCP 헤더','20',1.9,BLUE),('TCP 데이터','1460',7.75,TEAL)]
x=.78
for title,n,w,c in parts:
    box(s,x,3.7,w,1.38,c)
    text(s,x+.09,3.91,w-.18,.43,title,21,WHITE,True,PP_ALIGN.CENTER)
    text(s,x+.09,4.4,w-.18,.38,n,23,WHITE,True,PP_ALIGN.CENTER)
    x+=w+.08
text(s,.82,5.21,11.8,.37,'기본 헤더·옵션 없음 가정 / 도식의 폭은 개념 표현 / MAC 헤더·FCS 제외',13,MUTED)
box(s,.79,5.88,11.76,.8,WHITE)
text(s,1.05,6.03,3.78,.49,'recv(4096)',25,INK,True,font=MONO)
text(s,5.15,6.05,7.02,.43,'수신 호출의 최대 크기  ·  MTU 설정 아님',20,TEAL,True)
note(s,3,'1500은 모든 네트워크의 절대 최대가 아닙니다. 장비·링크·옵션·경로에 따라 다릅니다. 그림은 IPv4/TCP 기본 헤더의 산술 예시이며 실제 길이 비례 그림은 아닙니다. recv의 크기와 실제 반환 바이트 수도 구분합니다.')

# 07 — TCP endpoints
s = new(4,'서버는 기다리고, 클라이언트는 연결한다','클라이언트의 로컬 주소·포트는 보통 운영체제가 자동 선택')
laptop(s,1.05,2.35,.9)
server_icon(s,10.8,2.38,.85,TEAL,WHITE)
text(s,2.8,2.46,3.4,.51,'CLIENT',28,INK,True)
text(s,7.48,2.46,3.0,.51,'SERVER',28,INK,True)
pill(s,1.0,3.65,4.25,'socket → connect',size=21)
pill(s,7.6,3.65,4.65,'socket → bind → listen',size=19)
line(s,5.42,3.87,7.35,3.87,TEAL,3,True)
text(s,5.51,3.27,1.7,.4,'연결 요청',17,TEAL,True,PP_ALIGN.CENTER)
node(s,1.0,4.55,4.26,1.1,'send / recv',fill=WHITE,size=25)
node(s,7.59,4.55,4.66,1.1,'accept → recv / send',fill=WHITE,size=23)
line(s,5.5,4.93,7.35,4.93,BLUE,2,True)
line(s,7.35,5.31,5.5,5.31,ORANGE,2,True)
text(s,1.0,6.05,4.4,.43,'127.0.0.1 : 임시 포트',20,MUTED,False,PP_ALIGN.CENTER)
text(s,7.57,6.05,4.7,.43,'127.0.0.1 : 8000',22,TEAL,True,PP_ALIGN.CENTER)
note(s,4,'클라이언트도 특정 로컬 주소·포트를 요구하면 명시적으로 bind할 수 있습니다. 서버에서는 listen 소켓을 유지하고 accept가 반환한 연결 소켓으로 데이터를 주고받습니다. 그림의 send/recv는 방향 설명이며 실제 Python 구현은 sendall을 사용합니다.')

# 08 — TCP stream
s = new(4,'한 번에 도착한다는 보장은 없다','TCP는 메시지 묶음이 아닌 바이트 스트림')
box(s,.82,2.5,5.15,1.05,NAVY)
text(s,1.05,2.65,4.75,.65,'GET / HTTP/1.1\\r\\nHo',22,WHITE,font=MONO)
box(s,7.0,2.5,5.52,1.05,NAVY)
text(s,7.22,2.65,5.08,.65,'st: localhost\\r\\n\\r\\n',21,WHITE,font=MONO)
text(s,.85,3.64,4.9,.42,'첫 번째 수신 · 아직 대기',19,MUTED)
text(s,7.05,3.64,5.3,.42,'다음 수신 · 헤더 완성',19,TEAL,True)
line(s,3.38,4.19,5.17,4.81,TEAL,2,True)
line(s,9.77,4.19,8.12,4.81,TEAL,2,True)
box(s,2.2,4.85,8.9,1.08,PALE)
text(s,2.42,5.02,8.45,.63,'누적 → \\r\\n\\r\\n 탐색 → 파싱',27,TEAL,True,PP_ALIGN.CENTER)
pill(s,3.57,6.32,6.18,'시간 제한 + 헤더 크기 제한',fill=WHITE,fg=INK,size=21)
note(s,4,'실제 test_fragmented_headers의 입력을 시각화했습니다. 첫 조각만 보낸 동안에는 응답하지 않는지 확인한 후 나머지를 보내 200을 확인합니다. 이것은 응용프로그램의 분할 수신 예시이며 물리 패킷 수를 주장하는 그림이 아닙니다.')

# 09 — Directory map
s = new(5,'폴더마다 역할을 나누다','현재 디렉터리 기준 · 상세 파일 설명은 슬라이드 노트')
pill(s,4.44,2.18,4.44,'OJT_MANAGE/',fill=NAVY,fg=WHITE,size=25)
line(s,6.66,2.65,6.66,3.03,TEAL,2)
line(s,2.45,3.03,10.87,3.03,TEAL,2)
dirs=[('src/mini_http','서버 구현',.72,3.45),('tests','단위 · 통합 시험',4.93,3.45),('docs','계획 · 명령 · 기록',9.14,3.45),
      ('www','HTML · CSS · 이미지',.72,5.12),('examples','TCP Echo',4.93,5.12),('OJT','REPORT · PPT',9.14,5.12)]
for title,small,x,y in dirs:
    if y<4: line(s,x+1.73,3.03,x+1.73,3.43,TEAL,2)
    box(s,x,y,3.47,1.33,WHITE)
    folder(s,x+.2,y+.22,.53,TEAL)
    text(s,x+.99,y+.16,2.35,.48,title,19,INK,True)
    text(s,x+.2,y+.87,3.05,.3,small,17,MUTED)
note(s,5,'주요 루트 파일: README.md(실행 안내), pyproject.toml(패키지·시험 설정). src에는 __init__, __main__, config, server, request, handler, response가 있습니다. tests/unit의 test_request, test_response, test_handler와 integration/test_http_server가 각 기능을 확인합니다. fixtures는 입력 자료입니다. docs에는 통합 계획·실행/접속 안내와 스크립트가 있으며, OJT/REPORT에는 보고서, OJT/PPT에는 원고와 발표 자료가 있습니다. www/index.html, 404.html, assets/css/style.css, assets/images/connection.svg를 제공합니다. 디렉터리 전체는 ppt_manuscript.md의 파일 트리를 참고합니다.')

# 10 — Module map
s = new(5,'한 파일에 모든 책임을 넣지 않는다','네트워크 · 요청 해석 · 파일 처리 · 응답 생성 분리')
pill(s,.82,2.3,4.9,'__main__.py + config.py',fill=NAVY,fg=WHITE,size=22)
line(s,3.3,2.83,3.3,3.42,TEAL,2,True)
nodes=[(.83,3.47,2.62,'server.py','연결 · 바이트 수집'),(3.97,3.47,2.62,'request.py','요청 파싱'),
       (7.11,3.47,2.62,'handler.py','파일 · 상태 결정'),(10.25,3.47,2.25,'response.py','응답 조립')]
for x,y,w,title,small in nodes:
    node(s,x,y,w,1.64,title,small,size=20)
for a,b in [(3.53,3.87),(6.67,7.01),(9.81,10.15)]:line(s,a,4.2,b,4.2,TEAL,2,True)
pill(s,7.31,2.3,2.24,'www/',fill=PALE,fg=TEAL,size=23)
line(s,8.43,3.4,8.43,2.86,TEAL,2,True)
line(s,11.33,5.29,11.33,5.88,BLUE,2)
line(s,11.33,5.88,2.15,5.88,BLUE,2)
line(s,2.15,5.88,2.15,5.23,BLUE,2,True)
text(s,4.34,6.02,5.93,.52,'응답 바이트 → server.py → 클라이언트',20,BLUE,True,PP_ALIGN.CENTER)
note(s,5,'그림은 처리 관계를 단순화했습니다. 실제 호출 주체는 server.py입니다. parser가 요청 객체를 반환하면 handler를 호출하고, 반환된 Response를 바이트로 만들어 sendall합니다. 파싱 오류도 오류 응답으로 연결합니다. __init__.py는 패키지 설명, __main__.py는 설정 로드와 서버 시작, config.py는 주소·포트·루트·제한값 검증을 담당합니다.')

# 11 — Runtime
s = new(6,'실행: 연결에서 응답까지','정적 GET · 한 연결에 첫 요청 하나 · Connection: close')
labels=[('01','연결 수락','accept'),('02','헤더 수집','recv'),('03','요청 해석','parse'),
        ('06','연결 종료','close'),('05','응답 전송','sendall'),('04','파일·상태','handle')]
for i,(n,title,call) in enumerate(labels):
    x=.9+(i%3)*4.16; y=2.5+(i//3)*2.35
    box(s,x,y,3.32,1.42,WHITE)
    tag(s,x+.16,y+.17,n)
    text(s,x+.73,y+.13,2.4,.52,title,24,INK,True)
    text(s,x+.2,y+.85,2.9,.36,call,19,MUTED,False,PP_ALIGN.CENTER,MONO)
for y in (3.21,5.56):
    if y<4:
        line(s,4.35,y,4.91,y,TEAL,2,True); line(s,8.51,y,9.07,y,TEAL,2,True)
    else:
        line(s,4.91,y,4.35,y,TEAL,2,True); line(s,9.07,y,8.51,y,TEAL,2,True)
line(s,10.88,4.02,10.88,4.69,TEAL,2,True)
pill(s,4.67,4.06,3.97,'오류 → 오류 응답',fill='FFF0DD',fg='9A5C18',size=19)
line(s,.68,5.52,.48,5.52,BLUE,2)
line(s,.48,5.52,.48,3.21,BLUE,2)
line(s,.48,3.21,.78,3.21,BLUE,2,True)
note(s,6,'설정 검증 후 socket→bind→listen을 수행하고 이 반복 흐름에 진입합니다. 헤더 종료·시간·크기를 확인하며 정상 파싱 후 파일 및 상태를 결정합니다. 오류는 해당 오류 응답으로 처리하며 빈 연결 종료나 전송 단절은 정리합니다. Ctrl+C는 수신 대기 소켓도 종료합니다.\n시연 명령(터미널 1): /home/yoondh/work/OJT_MANAGE/docs/ojt_manage_run_server\n터미널 2: /home/yoondh/work/OJT_MANAGE/docs/ojt_manage_run_client\n없는 경로: 위 클라이언트 명령 뒤에 http://127.0.0.1:8000/not-found 추가. 정상 200과 없는 경로 404를 확인합니다.')

# 12 — Tests
s = new(6,'시험: 내부 함수와 실제 연결을 나누다','서버 시작·정리까지 자동화 · 실행 결과는 관리 기록에 반영')
pill(s,4.71,2.27,3.93,'pytest',fill=NAVY,fg=WHITE,size=27)
line(s,6.67,2.76,6.67,3.08,TEAL,2)
line(s,2.61,3.08,10.74,3.08,TEAL,2)
for x,title,key,sub in [(.76,'단위','파서 · 응답 · 파일','함수의 반환값'),(4.86,'소켓 통합','임시 포트 · 실제 요청','상태 · 본문 · 종료'),(8.96,'실행 통합','별도 프로세스','설정 오류 · Ctrl+C')]:
    line(s,x+1.85,3.08,x+1.85,3.43,TEAL,2)
    box(s,x,3.45,3.64,1.83,WHITE)
    text(s,x+.19,3.63,3.22,.48,title,25,INK,True,PP_ALIGN.CENTER)
    text(s,x+.15,4.25,3.34,.41,key,19,TEAL,True,PP_ALIGN.CENTER)
    text(s,x+.15,4.78,3.34,.31,sub,17,MUTED,False,PP_ALIGN.CENTER)
    line(s,x+1.82,5.4,x+1.82,5.7,TEAL,2)
line(s,2.58,5.7,10.78,5.7,TEAL,2)
line(s,6.67,5.7,6.67,6.05,TEAL,2,True)
pill(s,3.15,6.12,7.02,'비교 → 자원 정리 → 결과 확인 → 기록',fill=PALE,fg=TEAL,size=21)
note(s,6,'이 분기는 시험 종류를 구분하며 병렬 실행을 뜻하지 않습니다. 소켓 통합 시험은 테스트가 임시 포트 서버를 준비하고 성공·실패와 관계없이 정리합니다. 서버 기능은 순차 처리입니다. 실행 통합 시험에는 설정 오류와 별도 프로세스의 정상 종료 등이 있습니다. 기록 갱신은 사람이 결과를 확인한 뒤 수행하며 pytest가 자동 작성하지 않습니다.\n시험 명령: cd /home/yoondh/work/OJT_MANAGE && .venv/bin/python3 -m pytest tests/ -q')

# 13 — Project management
s = new(7,'서버를 만드는 과정도 관리한다','프로젝트 관리 교육의 15주 계획')
phases=[('01','계획','1–3주','목표 · 범위 · WBS',TEAL),('02','수행','4–7주','구현 · 기능 연결',BLUE),
        ('03','통제','8–11주','진척 · 시험 · 위험','64799A'),('04','변경·종료','12–15주','영향 · 승인 · 인계','A36A30')]
for i,(n,name,week,key,c) in enumerate(phases):
    x=.74+i*3.18
    box(s,x,2.54,2.99,3.17,WHITE)
    circle(s,x+.23,2.78,.7,c)
    text(s,x+.23,2.78,.7,.7,n,22,WHITE,True,PP_ALIGN.CENTER)
    text(s,x+.22,3.78,2.55,.6,name,30,INK,True)
    text(s,x+.22,4.6,2.55,.41,week,23,c,True)
    text(s,x+.22,5.18,2.62,.32,key,16,MUTED)
    if i<3:line(s,x+3.02,4.07,x+3.15,4.07,c,2,True)
text(s,.85,6.18,11.7,.47,'교육 일정 ≠ 수행 완료 기록',24,TEAL,True,PP_ALIGN.CENTER)
note(s,7,'15주 계획을 교육의 단계로 표현했습니다. 모든 주차를 수행했다는 의미는 아닙니다. 실제 시간·담당·승인 기록이 없는 항목에 임의의 완료율을 부여하지 않습니다.')

# 14 — Traceability
s = new(7,'완료는 증거로 판단한다','요구사항 → 작업 → 구현 → 시험 → 기록')
items=[('REQ-003','기본 HTML'),('WBS-004','응답 · 파일'),('handler.py','GET /'),('HTTP 시험','200 + 한글')]
for i,(title,detail) in enumerate(items):
    x=.75+i*3.2
    node(s,x,2.78,2.85,1.72,title,detail,fill=WHITE,size=23)
    if i<3:line(s,x+2.96,3.59,x+3.13,3.59,TEAL,3,True)
box(s,1.57,5.13,10.2,1.12,NAVY)
text(s,1.87,5.35,9.6,.66,'docs/ojt_manage_planning.md',28,WHITE,True,PP_ALIGN.CENTER,font=MONO)
line(s,11.3,4.68,11.3,4.91,TEAL,2)
line(s,11.3,4.91,6.67,4.91,TEAL,2)
line(s,6.67,4.91,6.67,5.1,TEAL,2,True)
note(s,7,'실제 관리 문서는 REQ-001~012와 WBS-001~011을 포함합니다. 이 슬라이드는 그중 REQ-003의 요구사항을 WBS-004, handler.py 및 test_get_with_http_client와 연결해 설명합니다. WBS는 작업 계획이며 실제 일정 완료율은 자료 없이 산정하지 않습니다.')

# 15 — Risk to test
s = new(7,'위험을 코드와 시험으로 연결한다','예상 문제 → 대응 → 재확인')
heads=[(.9,3.33,'위험'),(4.64,3.33,'대응'),(8.39,3.33,'시험')]
for x,w,t in heads:text(s,x,2.27,w,.44,t,18,MUTED,True)
risks=[('분할 수신','헤더 누적','분할 요청 시험'),('느린 입력','전체 수신 제한','시간 초과 · 복구'),('경로 탈출','루트 범위 검사','외부 경로 · 링크')]
for i,row in enumerate(risks):
    y=3.0+i*1.05
    for j,label in enumerate(row):
        x=.87+j*3.75
        box(s,x,y,3.3,.82,WHITE if j!=1 else PALE)
        text(s,x+.13,y+.12,3.04,.56,label,23,TEAL if j==1 else INK,True,PP_ALIGN.CENTER)
        if j<2:line(s,x+3.42,y+.41,x+3.62,y+.41,TEAL,2,True)
pill(s,2.6,6.49,8.13,'변경 시: 영향 분석 + 관련 회귀시험',fill=NAVY,fg=WHITE,size=21)
note(s,7,'이 프로젝트는 위험표를 실제 대응과 시험으로 연결했습니다. 실행 스크립트와 문서도 사용자 요청에 따라 보완했습니다. 동시 처리나 Keep-Alive 등 추가 기능은 확장 후보로 구분하고 변경 영향 분석 양식을 준비했습니다. 추가 서버 기능 승인이나 Git 기준선 태그 생성이 완료되었다고 주장하지 않습니다.')

# 16 — Evidence and remaining work
s = new(7,'확인한 결과와 남은 작업','2026.09.10 앞선 검증 기록 기준')
box(s,.8,2.43,5.46,3.74,NAVY)
text(s,1.14,2.69,4.72,.43,'자동 시험',22,CYAN,True)
text(s,1.07,3.25,4.9,1.3,'77',88,WHITE,True)
text(s,1.16,4.82,4.52,.65,'통과 · 실패 0',30,WHITE,True)
text(s,1.15,5.69,4.7,.28,'구현 + 로컬 자동 검증',16,'AEC0CE')
box(s,6.75,2.43,5.77,3.74,WHITE)
text(s,7.11,2.72,5.08,.43,'다음 검증',22,TEAL,True)
for i,label in enumerate(['LAN · 스마트폰','브라우저 · 패킷 관찰','실제 일정 · 승인 · 종료']):
    circle(s,7.11,3.63+i*.72,.12,ORANGE)
    text(s,7.42,3.39+i*.72,4.73,.6,label,24,INK,True)
text(s,.88,6.49,11.64,.33,'로컬 시험 통과와 프로젝트 전체 종료를 구분',19,MUTED,False,PP_ALIGN.CENTER)
note(s,7,'자동 시험 77개 통과, 실패 0은 앞선 2026년 9월 10일 실행 기록입니다. 본 PPT 제작에서는 서버 시험을 다시 실행하지 않았습니다. 관련 원문: OJT/REPORT/ojt_manage_raw_server_report.md 및 docs/ojt_manage_planning.md. 실행 시간은 성능 벤치마크가 아니므로 슬라이드에서 제외했습니다. 실제 다른 기기·화면·패킷 관찰과 일정 평가·승인은 미검증·대기 상태입니다.')

# 17 — Closing
s = new(8,'',dark=True)
network(s,7.96,1.47,4.43,4.63)
text(s,.75,2.13,8.2,1.24,'감사합니다.',62,WHITE,True)
text(s,.8,3.71,7.57,.68,'질문 부탁드립니다.',28,'B8CAD6')
shape(s,.82,4.79,.88,.055,CYAN)
text(s,.8,5.29,8.0,.43,'윤대현  ·  2024531046',23,WHITE,True)
text(s,.8,5.89,8.0,.35,'한국공학대학교 컴퓨터전자공학과',19,'B8CAD6')
note(s,8)

assert len(prs.slides)==17
# Catch off-slide content at generation time. Connector endpoints are included.
for index, slide in enumerate(prs.slides,1):
    for obj in slide.shapes:
        assert obj.left >= 0 and obj.top >= 0, (index,obj.name,'negative position')
        assert obj.left+obj.width <= prs.slide_width+Inches(.02), (index,obj.name,'right overflow')
        assert obj.top+obj.height <= prs.slide_height+Inches(.02), (index,obj.name,'bottom overflow')
    assert slide.notes_slide.notes_text_frame.text.strip()
target = OUT / 'raw_server.pptx'
prs.save(target)
print(target)
print(f'{len(prs.slides)} slides, editable vector diagrams, Korean notes included')
