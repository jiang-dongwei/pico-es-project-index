#!/usr/bin/env python3
"""Build original 6502 test tapes; no BASIC ROM or third-party game data.

The tiny assembler only encodes the instructions used below, checks all
relative branches and emits deterministic TAPs plus the embedded demo header.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import argparse


class Program:
    def __init__(self):
        self.code = bytearray()
        self.labels = {}
        self.fixups = []

    def emit(self, *data):
        self.code.extend(data)

    def label(self, name):
        if name in self.labels:
            raise ValueError(f"duplicate label {name}")
        self.labels[name] = 0x600 + len(self.code)

    def imm(self, op, value):
        self.emit(op, value)

    def absolute(self, op, address):
        self.emit(op, address & 255, address >> 8)

    def branch(self, op, target):
        self.fixups.append((len(self.code), target, True))
        self.emit(op, 0)

    def jump(self, target):
        self.fixups.append((len(self.code), target, False))
        self.emit(0x4c, 0, 0)

    def store(self, address, value):
        self.imm(0xa9, value)
        self.absolute(0x8d, address)

    def ay(self, reg, value):
        self.store(0x30f, reg)
        self.store(0x30c, 0xff)
        self.store(0x30c, 0xcc)
        self.store(0x30f, value)
        self.store(0x30c, 0xfc)
        self.store(0x30c, 0xcc)

    def finish(self):
        for offset, target, relative in self.fixups:
            address = self.labels[target]
            if relative:
                delta = address - (0x600 + offset + 2)
                if not -128 <= delta <= 127:
                    raise ValueError(f"branch out of range: {target}")
                self.code[offset + 1] = delta & 255
            else:
                self.code[offset + 1:offset + 3] = address.to_bytes(2, "little")
        return self.code


def make_program(hires=False):
    p = Program()
    p.emit(0x78, 0xd8, 0xa2, 0xff, 0x9a)  # SEI, CLD, LDX #ff, TXS
    p.store(0x303, 255)
    p.store(0x302, 0xf7)
    p.store(0x300, 4)
    p.store(0x30c, 0xcc)
    p.ay(7, 0x7e)  # channel A tone, keyboard port A output
    p.ay(0, 142)   # about 440 Hz
    p.ay(1, 0)
    p.ay(8, 0)
    # Clear all 28 text rows to spaces using five bounded partial pages.
    p.imm(0xa2, 0)
    p.imm(0xa9, 32)
    p.label("clear")
    for address in (0xbb80, 0xbc80, 0xbd80, 0xbe80):
        p.absolute(0x9d, address)
    p.emit(0xe8)
    p.branch(0xd0, "clear")
    p.imm(0xa2, 0)
    p.label("tail")
    p.absolute(0x9d, 0xbf80)
    p.emit(0xe8)
    p.imm(0xe0, 0x60)
    p.branch(0xd0, "tail")
    # Original 6x8 glyph patterns needed by this test.
    font = {
        ' ': [0]*8, 'O': [30,33,33,33,33,33,30,0],
        'R': [62,33,33,62,36,34,33,0], 'I': [31,4,4,4,4,4,31,0],
        'C': [30,33,32,32,32,33,30,0], 'T': [63,12,12,12,12,12,12,0],
        'E': [63,32,32,62,32,32,63,0], 'S': [31,32,32,30,1,1,62,0],
        'L': [32,32,32,32,32,32,63,0], 'F': [63,32,32,62,32,32,32,0],
        'G': [30,33,32,39,33,33,30,0], 'H': [33,33,33,63,33,33,33,0],
        'A': [30,33,33,63,33,33,33,0], 'P': [62,33,33,62,32,32,32,0],
        'D': [62,33,33,33,33,33,62,0], 'M': [33,51,45,33,33,33,33,0],
        'U': [33,33,33,33,33,33,30,0], 'V': [33,33,33,33,18,18,12,0],
        'B': [62,33,33,62,33,33,62,0],
        'N': [33,49,41,37,35,33,33,0], 'Y': [33,33,18,12,12,12,12,0],
        'X': [33,18,12,12,12,18,33,0],
    }
    for char, rows in font.items():
        for i, bits in enumerate(rows):
            p.store((0x9800 if hires else 0xb400) + ord(char)*8 + i, bits)
    if not hires:
        for row, message in ((3, "ORIC TEST"), (5, "ORIGINAL TEST PROGRAM"),
                             (20, "LEFT RIGHT MOVE"), (22, "A SPACE TONE"),
                             (26, "MENU HOME SETTINGS")):
            for i, char in enumerate(message):
                p.store(0xbb80 + row*40 + 5 + i, ord(char))
    p.store(0x20, 18)  # player X, inspected by host tests
    p.store(0x30b, 0x40)
    p.store(0x304, 0xfe)
    p.store(0x305, 0x4d)
    if hires:
        # Copy an explicit, human-readable bitmap into A000..BFFF. The last
        # 1152 bytes also back the HIRES text overlay at BB80..BFFF.
        source_lo = len(p.code) + 1
        p.store(0x24, 0)
        source_hi = len(p.code) + 1
        p.store(0x25, 0)
        p.store(0x22, 0)
        p.store(0x23, 0xa0)
        p.imm(0xa2, 32)
        p.imm(0xa0, 0)
        p.label("hires_copy")
        p.emit(0xb1, 0x24, 0x91, 0x22, 0xc8)
        p.branch(0xd0, "hires_copy")
        p.emit(0xe6, 0x25, 0xe6, 0x23, 0xca)
        p.branch(0xd0, "hires_copy")
        p.store(0xbb80, 30)  # enter HIRES 50 Hz from the power-up text mode
        p.store(0xa000, 30)  # keep HIRES selected at frame start
        p.ay(6, 9)
        p.ay(7, 0x77)       # noise A only, port A output
        p.ay(8, 11)
    p.label("frame")
    p.absolute(0xad, 0x30d)
    p.imm(0x29, 0x40)
    p.branch(0xf0, "frame")
    p.absolute(0xad, 0x304)
    p.emit(0xe6, 0x21)  # running frame heartbeat in guest RAM
    if hires:
        pass  # preserve the chart's per-line color attributes
    else:
        p.emit(0xa6, 0x20, 0xa9, 32)
        p.absolute(0x9d, 0xbb80 + 15*40)
        p.ay(14, 0xdf)  # row4, column5 = LEFT
        p.absolute(0xad, 0x300)
        p.imm(0x29, 8)
        p.branch(0xf0, "right")
        p.emit(0xa5, 0x20, 0xc9, 2)
        p.branch(0x90, "right")
        p.emit(0xc6, 0x20)
        p.label("right")
        p.ay(14, 0x7f)  # RIGHT
        p.absolute(0xad, 0x300)
        p.imm(0x29, 8)
        p.branch(0xf0, "player")
        p.emit(0xa5, 0x20, 0xc9, 37)
        p.branch(0xb0, "player")
        p.emit(0xe6, 0x20)
        p.label("player")
        p.emit(0xa6, 0x20, 0xa9, ord('X'))
        p.absolute(0x9d, 0xbb80 + 15*40)
        p.ay(14, 0xfe)  # SPACE
        p.absolute(0xad, 0x300)
        p.imm(0x29, 8)
        p.branch(0xf0, "silent")
        p.ay(8, 13)
        p.jump("frame")
        p.label("silent")
        p.ay(8, 0)
    p.jump("frame")
    if hires:
        p.label("image")
        p.emit(*make_hires_image(font))
    code = p.finish()
    if hires:
        address = p.labels["image"]
        code[source_lo] = address & 255
        code[source_hi] = address >> 8
    return code


def make_hires_image(font):
    pixels = [[0] * 228 for _ in range(176)]

    def rect(x0, y0, x1, y1):
        for y in range(max(0, y0), min(176, y1)):
            for x in range(max(0, x0), min(228, x1)):
                pixels[y][x] = 1

    def label(message, x0, y0, scale):
        for char in message:
            for row, bits in enumerate(font[char]):
                for bit in range(6):
                    if bits & (32 >> bit):
                        rect(x0 + bit*scale, y0 + row*scale,
                             x0 + (bit+1)*scale, y0 + (row+1)*scale)
            x0 += 7*scale

    rect(0, 0, 228, 2); rect(0, 174, 228, 176)
    rect(0, 0, 2, 176); rect(226, 0, 228, 176)
    label('ORIC HIRES', 40, 22, 2)
    label('PICOES TEST', 56, 62, 1)
    for x in range(14, 214, 20):
        rect(x, 107, x+10, 147)
    for y in range(103, 151):
        pixels[y][min(227, 14 + (y-103)*4)] = 1

    # $20 is a space in text mode, but lights bit 5 in HIRES and creates the
    # vertical comb seen below the chart. $40 is blank pixels and an unused
    # glyph in the HIRES text character set.
    image = bytearray([0x40] * 8192)
    for y in range(176):
        start = y*40
        image[start] = 30  # repeat HIRES mode at each raster line
        image[start+1] = (1, 3, 2, 6, 7)[y // 36]  # ink color band
        for column in range(38):
            bits = sum(pixels[y][column*6+bit] << (5-bit)
                       for bit in range(6))
            image[start+column+2] = 0x40 | bits
    for row, message in ((25, 'ORIC HIRES TEST'), (26, 'PICOES VIDEO CHECK')):
        offset = 0x1b80 + row*40 + (40-len(message))//2
        image[offset:offset+len(message)] = message.encode('ascii')
    return image


def make_audio_program():
    p = Program()
    p.emit(0x78, 0xd8, 0xa2, 0xff, 0x9a)  # SEI, CLD, LDX #ff, TXS
    p.store(0x303, 255)
    p.store(0x302, 0xf7)
    p.store(0x300, 4)
    p.store(0x30c, 0xcc)
    p.ay(7, 0x7e)
    p.ay(1, 0)
    p.ay(8, 0)
    p.imm(0xa2, 0)
    p.imm(0xa9, 32)
    p.label('clear_audio')
    for address in (0xbb80, 0xbc80, 0xbd80, 0xbe80):
        p.absolute(0x9d, address)
    p.emit(0xe8)
    p.branch(0xd0, 'clear_audio')
    p.imm(0xa2, 0)
    p.label('clear_audio_tail')
    p.absolute(0x9d, 0xbf80)
    p.emit(0xe8)
    p.imm(0xe0, 0x60)
    p.branch(0xd0, 'clear_audio_tail')
    for row, message in ((3, 'ORIC AUDIO TEST'),
                         (5, 'ORIGINAL TONE SEQUENCE'),
                         (8, 'C D E F G A B C SILENCE'),
                         (11, 'CURRENT NOTE:'),
                         (23, 'EACH STEP HALF A SECOND'),
                         (25, 'LISTEN FOR GAPS OR CRACKLE')):
        for column, char in enumerate(message):
            p.store(0xbb80 + row*40 + 5 + column, ord(char))
    p.store(0x30b, 0x40)
    p.store(0x304, 0xfe)
    p.store(0x305, 0x4d)
    p.imm(0xa2, 0)
    p.imm(0xa0, 0)
    p.label('frame')
    p.absolute(0xad, 0x30d)
    p.imm(0x29, 0x40)
    p.branch(0xf0, 'frame')
    p.absolute(0xad, 0x304)
    p.emit(0xc8)
    p.imm(0xc0, 25)
    p.branch(0xd0, 'frame')
    p.imm(0xa0, 0)
    period_operand = len(p.code) + 1
    p.absolute(0xbd, 0)
    p.absolute(0x8d, 0x20)
    name_operand = len(p.code) + 1
    p.absolute(0xbd, 0)
    p.absolute(0x8d, 0xbb80 + 11*40 + 19)
    volume_operand = len(p.code) + 1
    p.absolute(0xbd, 0)
    p.absolute(0x8d, 0x21)
    for reg, address in ((0, 0x20), (8, 0x21)):
        p.store(0x30f, reg)
        p.store(0x30c, 0xff)
        p.store(0x30c, 0xcc)
        p.absolute(0xad, address)
        p.absolute(0x8d, 0x30f)
        p.store(0x30c, 0xfc)
        p.store(0x30c, 0xcc)
    p.emit(0xe8)
    p.imm(0xe0, 9)
    p.branch(0xd0, 'continue')
    p.imm(0xa2, 0)
    p.label('continue')
    p.jump('frame')
    p.label('periods')
    p.emit(239, 213, 189, 179, 159, 142, 127, 119, 142)
    p.label('names')
    p.emit(*b'CDEFGABCS')
    p.label('volumes')
    p.emit(13, 13, 13, 13, 13, 13, 13, 13, 0)
    code = p.finish()
    for offset, label in ((period_operand, 'periods'),
                          (name_operand, 'names'), (volume_operand, 'volumes')):
        code[offset:offset+2] = p.labels[label].to_bytes(2, 'little')
    return code


def tape(code, name):
    end = 0x600 + len(code) - 1
    return (b'\x16\x16\x16\x24\x00\x00\x80\xc7' +
            end.to_bytes(2, 'big') + b'\x06\x00\x00' +
            name.encode('ascii') + b'\x00' + code)


def generate(directory, header=None):
    directory.mkdir(parents=True, exist_ok=True)
    demo = tape(make_program(), 'PICOES TEST')
    (directory / 'PicoES_Oric_Controls.tap').write_bytes(demo)
    hires = tape(make_program(True), 'PICOES HIRES')
    (directory / 'PicoES_Oric_Hires.tap').write_bytes(hires)
    audio = tape(make_audio_program(), 'PICOES AUDIO')
    (directory / 'PicoES_Oric_Audio.tap').write_bytes(audio)
    if header:
        lines = ["/* Generated by tools/make_test_tapes.py; GPL-3.0-or-later. */"]
        for name, data in (("oric_demo_tape", demo), ("oric_hires_tape", hires)):
            lines += [f"static const unsigned char {name}[] = {{"]
            lines += ["    " + ','.join(f'0x{x:02x}' for x in data[i:i+16]) + ','
                      for i in range(0, len(data), 16)]
            lines += ['};', '']
        header.write_text('\n'.join(lines), encoding='ascii')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--header', type=Path)
    args = parser.parse_args()
    generate(args.output, args.header)
