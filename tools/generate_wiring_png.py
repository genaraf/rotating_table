#!/usr/bin/env python3
import struct
import zlib

W, H = 1600, 1000

WHITE = (250, 252, 255)
BLACK = (20, 30, 35)
GRAY = (110, 125, 135)
BLUE = (36, 99, 161)
GREEN = (34, 139, 88)
ORANGE = (214, 137, 16)
RED = (180, 48, 48)
PURPLE = (120, 70, 170)
BOX_BG = (238, 244, 248)

img = [[WHITE for _ in range(W)] for _ in range(H)]

FONT = {
    'A':[0x0E,0x11,0x11,0x1F,0x11,0x11,0x11],'B':[0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E],
    'C':[0x0E,0x11,0x10,0x10,0x10,0x11,0x0E],'D':[0x1E,0x11,0x11,0x11,0x11,0x11,0x1E],
    'E':[0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F],'F':[0x1F,0x10,0x10,0x1E,0x10,0x10,0x10],
    'G':[0x0E,0x11,0x10,0x17,0x11,0x11,0x0E],'H':[0x11,0x11,0x11,0x1F,0x11,0x11,0x11],
    'I':[0x1F,0x04,0x04,0x04,0x04,0x04,0x1F],'J':[0x07,0x02,0x02,0x02,0x12,0x12,0x0C],
    'K':[0x11,0x12,0x14,0x18,0x14,0x12,0x11],'L':[0x10,0x10,0x10,0x10,0x10,0x10,0x1F],
    'M':[0x11,0x1B,0x15,0x15,0x11,0x11,0x11],'N':[0x11,0x19,0x15,0x13,0x11,0x11,0x11],
    'O':[0x0E,0x11,0x11,0x11,0x11,0x11,0x0E],'P':[0x1E,0x11,0x11,0x1E,0x10,0x10,0x10],
    'Q':[0x0E,0x11,0x11,0x11,0x15,0x12,0x0D],'R':[0x1E,0x11,0x11,0x1E,0x14,0x12,0x11],
    'S':[0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E],'T':[0x1F,0x04,0x04,0x04,0x04,0x04,0x04],
    'U':[0x11,0x11,0x11,0x11,0x11,0x11,0x0E],'V':[0x11,0x11,0x11,0x11,0x11,0x0A,0x04],
    'W':[0x11,0x11,0x11,0x15,0x15,0x15,0x0A],'X':[0x11,0x11,0x0A,0x04,0x0A,0x11,0x11],
    'Y':[0x11,0x11,0x0A,0x04,0x04,0x04,0x04],'Z':[0x1F,0x01,0x02,0x04,0x08,0x10,0x1F],
    '0':[0x0E,0x11,0x13,0x15,0x19,0x11,0x0E],'1':[0x04,0x0C,0x04,0x04,0x04,0x04,0x0E],
    '2':[0x0E,0x11,0x01,0x02,0x04,0x08,0x1F],'3':[0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E],
    '4':[0x02,0x06,0x0A,0x12,0x1F,0x02,0x02],'5':[0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E],
    '6':[0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E],'7':[0x1F,0x01,0x02,0x04,0x08,0x08,0x08],
    '8':[0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E],'9':[0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E],
    '-':[0x00,0x00,0x00,0x1F,0x00,0x00,0x00],'>':[0x00,0x10,0x08,0x04,0x08,0x10,0x00],
    '+':[0x00,0x04,0x04,0x1F,0x04,0x04,0x00],'/':[0x01,0x02,0x04,0x08,0x10,0x00,0x00],
    '(':[0x02,0x04,0x08,0x08,0x08,0x04,0x02],')':[0x08,0x04,0x02,0x02,0x02,0x04,0x08],
    '.':[0x00,0x00,0x00,0x00,0x00,0x06,0x06],':':[0x00,0x06,0x06,0x00,0x06,0x06,0x00],
    '=':[0x00,0x1F,0x00,0x1F,0x00,0x00,0x00],' ':[0x00,0x00,0x00,0x00,0x00,0x00,0x00],
}


def put(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        img[y][x] = c


def fill_rect(x, y, w, h, c):
    for yy in range(y, y + h):
        if 0 <= yy < H:
            row = img[yy]
            x1 = max(0, x)
            x2 = min(W, x + w)
            for xx in range(x1, x2):
                row[xx] = c


def rect(x, y, w, h, c, t=2):
    fill_rect(x, y, w, t, c)
    fill_rect(x, y + h - t, w, t, c)
    fill_rect(x, y, t, h, c)
    fill_rect(x + w - t, y, t, h, c)


def line(x1, y1, x2, y2, c, thick=2):
    dx = abs(x2 - x1)
    dy = abs(y2 - y1)
    sx = 1 if x1 < x2 else -1
    sy = 1 if y1 < y2 else -1
    err = dx - dy
    while True:
        for ty in range(-thick // 2, thick // 2 + 1):
            for tx in range(-thick // 2, thick // 2 + 1):
                put(x1 + tx, y1 + ty, c)
        if x1 == x2 and y1 == y2:
            break
        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x1 += sx
        if e2 < dx:
            err += dx
            y1 += sy


def arrow(x1, y1, x2, y2, c, thick=3):
    line(x1, y1, x2, y2, c, thick)
    if abs(x2 - x1) >= abs(y2 - y1):
        sign = 1 if x2 > x1 else -1
        line(x2, y2, x2 - 14 * sign, y2 - 8, c, thick)
        line(x2, y2, x2 - 14 * sign, y2 + 8, c, thick)
    else:
        sign = 1 if y2 > y1 else -1
        line(x2, y2, x2 - 8, y2 - 14 * sign, c, thick)
        line(x2, y2, x2 + 8, y2 - 14 * sign, c, thick)


def text(x, y, s, c=BLACK, scale=3):
    xx = x
    for ch in s.upper():
        glyph = FONT.get(ch, FONT[' '])
        for row_idx, row_bits in enumerate(glyph):
            for bit in range(5):
                if row_bits & (1 << (4 - bit)):
                    fill_rect(xx + bit * scale, y + row_idx * scale, scale, scale, c)
        xx += 6 * scale


# Background sections
fill_rect(0, 0, W, H, WHITE)
text(360, 30, "WIRING DIAGRAM M5STAMP-C3 / ULN2003 / 28BYJ-48 / SERVO", BLACK, 4)
text(470, 72, "GPIO: 4 5 6 7 8 10", GRAY, 3)

# Device boxes
rect(80, 150, 320, 620, BLUE, 4)
fill_rect(84, 154, 312, 612, BOX_BG)
text(130, 180, "M5STAMP-C3", BLUE, 4)

rect(540, 170, 320, 420, GREEN, 4)
fill_rect(544, 174, 312, 412, BOX_BG)
text(615, 200, "ULN2003", GREEN, 4)

rect(1040, 190, 320, 220, ORANGE, 4)
fill_rect(1044, 194, 312, 212, BOX_BG)
text(1070, 220, "28BYJ-48 MOTOR", ORANGE, 4)

rect(1040, 470, 320, 190, PURPLE, 4)
fill_rect(1044, 474, 312, 182, BOX_BG)
text(1140, 500, "SERVO", PURPLE, 4)

rect(1040, 730, 320, 160, RED, 4)
fill_rect(1044, 734, 312, 152, BOX_BG)
text(1080, 760, "BUTTON (GPIO10)", RED, 3)
text(1080, 805, "SHORT: POWER", RED, 3)
text(1080, 845, "HOLD>5S: RESET", RED, 3)

# M5 pins labels
text(120, 260, "GPIO4", BLUE, 3)
text(120, 315, "GPIO5", BLUE, 3)
text(120, 370, "GPIO6", BLUE, 3)
text(120, 425, "GPIO7", BLUE, 3)
text(120, 500, "GPIO8 PWM", BLUE, 3)
text(120, 580, "GPIO10 BTN", BLUE, 3)
text(120, 660, "GND", BLUE, 3)

# ULN pins
text(590, 280, "IN1", GREEN, 3)
text(590, 335, "IN2", GREEN, 3)
text(590, 390, "IN3", GREEN, 3)
text(590, 445, "IN4", GREEN, 3)
text(590, 520, "GND", GREEN, 3)
text(710, 520, "VCC 5V", GREEN, 3)

# Connections stepper ctrl
arrow(300, 270, 580, 290, BLUE, 3)
arrow(300, 325, 580, 345, BLUE, 3)
arrow(300, 380, 580, 400, BLUE, 3)
arrow(300, 435, 580, 455, BLUE, 3)

# ULN to motor
arrow(860, 300, 1035, 300, ORANGE, 4)
text(860, 330, "5-WIRE MOTOR CABLE", ORANGE, 2)

# Servo signal
arrow(320, 510, 1035, 540, PURPLE, 3)
text(720, 545, "PWM SIGNAL", PURPLE, 2)

# Button
arrow(340, 590, 1035, 770, RED, 3)

# Ground and power rails
line(290, 670, 1400, 670, BLACK, 3)
text(670, 685, "COMMON GND", BLACK, 3)
arrow(780, 670, 780, 585, BLACK, 3)
arrow(1160, 670, 1160, 640, BLACK, 3)
arrow(700, 670, 700, 545, BLACK, 3)
arrow(1160, 670, 1160, 405, BLACK, 3)

line(480, 915, 1400, 915, ORANGE, 4)
text(760, 930, "EXTERNAL +5V", ORANGE, 3)
arrow(700, 915, 700, 560, ORANGE, 3)
arrow(1180, 915, 1180, 650, ORANGE, 3)

text(70, 915, "NOTE: ALL GROUNDS MUST BE CONNECTED TOGETHER", GRAY, 3)


def png_chunk(tag, data):
    return struct.pack("!I", len(data)) + tag + data + struct.pack("!I", zlib.crc32(tag + data) & 0xFFFFFFFF)

raw = bytearray()
for y in range(H):
    raw.append(0)
    for (r, g, b) in img[y]:
        raw.extend((r, g, b))

ihdr = struct.pack("!IIBBBBB", W, H, 8, 2, 0, 0, 0)
idat = zlib.compress(bytes(raw), 9)
out = bytearray(b"\x89PNG\r\n\x1a\n")
out += png_chunk(b"IHDR", ihdr)
out += png_chunk(b"IDAT", idat)
out += png_chunk(b"IEND", b"")

with open("/home/gena/work/Rtable/docs/wiring-diagram.png", "wb") as f:
    f.write(out)

print("written: /home/gena/work/Rtable/docs/wiring-diagram.png")
