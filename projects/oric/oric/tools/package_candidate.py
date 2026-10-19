#!/usr/bin/env python3
"""Validate and stage Oric candidate locally; never touches an SD/BOOTSEL volume.
SPDX-License-Identifier: GPL-3.0-or-later
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import zlib
from make_test_tapes import generate

ROOT = Path(__file__).resolve().parents[1]


def verify_uf2(path, origin):
    data = path.read_bytes()
    if not data or len(data) % 512:
        raise ValueError('UF2 length')
    addresses = []
    numbers = []
    for offset in range(0, len(data), 512):
        magic0, magic1, flags, addr, size, number, total, family = struct.unpack_from('<8I', data, offset)
        assert (magic0, magic1) == (0x0A324655, 0x9E5D5157)
        assert struct.unpack_from('<I', data, offset+508)[0] == 0x0AB16F30
        assert size == 256 and flags & 0x2000
        if family == 0xE48BFF57:
            # SDK 2.3 RP2350 absolute-family compatibility block, not ARM payload.
            assert addr == 0x10FFFF00 and flags == 0xA000
            continue
        assert family == 0xE48BFF59 and flags == 0x2000
        assert origin <= addr < 0x11000000 and addr % 256 == 0
        assert number < total
        numbers.append(number)
        addresses.append(addr)
    assert addresses and min(addresses) == origin
    assert len(set(addresses)) == len(addresses)
    assert sorted(addresses) == list(range(origin, max(addresses)+256, 256))
    assert sorted(numbers) == list(range(len(numbers)))
    return {'armPayloadStart': hex(origin), 'armPayloadEnd': hex(max(addresses)+256),
            'armBlocks': len(addresses), 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest()}


def cover(path):
    # Original geometric keyboard artwork and hand-written bitmap lettering.
    w, h = 320, 240
    pixels = bytearray([20, 25, 32] * w * h)

    def rect(x, y, width, height, color):
        for row in range(y, y+height):
            for col in range(x, x+width):
                pixels[(row*w+col)*3:(row*w+col)*3+3] = bytes(color)

    font = {'O':[14,17,17,17,17,17,14], 'R':[30,17,17,30,20,18,17],
            'I':[31,4,4,4,4,4,31], 'C':[14,17,16,16,16,17,14],
            'A':[14,17,17,31,17,17,17], 'T':[31,4,4,4,4,4,4],
            'M':[17,27,21,21,17,17,17], 'S':[15,16,16,14,1,1,30],
            '1':[4,12,4,4,4,4,14], '/':[1,2,2,4,8,8,16], ' ':[0]*7}

    def text(value, x, y, scale, color):
        for char in value:
            for row, bits in enumerate(font[char]):
                for col in range(5):
                    if bits & (16 >> col):
                        rect(x+col*scale, y+row*scale, scale, scale, color)
            x += 6*scale

    text('ORIC', 64, 24, 8, (244, 237, 220))
    text('1 / ATMOS', 81, 93, 3, (231, 74, 65))
    rect(30, 137, 260, 80, (210, 49, 43))
    rect(40, 148, 240, 57, (28, 31, 35))
    for row in range(3):
        for col in range(12):
            rect(45+col*19, 152+row*13, 15, 9, (240, 230, 208))
    rect(101, 194, 112, 7, (240, 230, 208))
    raw = b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))

    def chunk(kind, payload):
        return struct.pack('>I', len(payload))+kind+payload+struct.pack('>I', zlib.crc32(kind+payload))

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>2I5B', w,h,8,2,0,0,0))+
                     chunk(b'IDAT', zlib.compress(raw,9))+chunk(b'IEND',b''))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--candidate', choices=['oric-candidate1','oric-candidate2','oric-diagnostic3','oric-visual4','oric-diagnostic5','oric-viafast6','oric-buffer7','oric-packed8','oric-speed9','oric-cache10','oric-viasteady11','oric-clock12','oric-presentskip13','oric-clock43214','oric-profile15','oric-browserdiag16','oric-inputcache17','oric-vialazy18','oric-viabudget19','oric-profilefix20','oric-profilephase21','oric-profilesample22','oric-release23','oric-xipguard24','oric-targetfps25','oric-fastload26','oric-rowcache27','oric-gameprofile29','oric-directpack30','oric-viaquiet32'], default='oric-candidate2')
    candidate = parser.parse_args().candidate
    destination = ROOT/'_build/delivery'/candidate
    destination.mkdir(parents=True, exist_ok=True)
    metadata = {'candidate': candidate, 'hardwareAccepted': False,
                'frontendContractVersion': 2, 'images': {}}
    for image, origin in [('loader',0x10080000),('standalone',0x10000000)]:
        source = ROOT/f'_build/pico-{image}/oric.uf2'
        metadata['images'][image] = verify_uf2(source, origin)
        target_name = 'oric.uf2' if candidate in ('oric-buffer7','oric-packed8','oric-speed9','oric-cache10','oric-viasteady11','oric-clock12','oric-presentskip13','oric-clock43214','oric-profile15','oric-browserdiag16','oric-inputcache17','oric-vialazy18','oric-viabudget19','oric-profilefix20','oric-profilephase21','oric-profilesample22','oric-release23','oric-xipguard24','oric-targetfps25','oric-fastload26','oric-rowcache27','oric-gameprofile29','oric-directpack30','oric-viaquiet32') and image == 'loader' else f'oric-{image}.uf2'
        shutil.copy2(source, destination/target_name)
    generate(destination/'roms/ORIC')
    cover(ROOT/'assets/loader/oric.png')
    shutil.copy2(ROOT/'assets/loader/oric.png', destination/'oric.png')
    shutil.copy2(ROOT/'doc/SOURCES.md', destination/'SOURCES.md')
    if candidate in ('oric-diagnostic3', 'oric-visual4', 'oric-diagnostic5', 'oric-viafast6', 'oric-buffer7', 'oric-packed8', 'oric-speed9', 'oric-cache10', 'oric-viasteady11', 'oric-clock12', 'oric-presentskip13', 'oric-clock43214', 'oric-profile15', 'oric-browserdiag16', 'oric-inputcache17', 'oric-vialazy18', 'oric-viabudget19', 'oric-profilefix20', 'oric-profilephase21', 'oric-profilesample22', 'oric-release23', 'oric-xipguard24', 'oric-targetfps25', 'oric-fastload26', 'oric-rowcache27', 'oric-gameprofile29', 'oric-directpack30', 'oric-viaquiet32'):
        if candidate == 'oric-viaquiet32':
            heading = ('# Oric VIA quiet shortcut 32\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. This unprofiled candidate '
                       'bypasses repeated input preparation and VIA dispatch only '
                       'during a previously proven quiet timer interval with no I/O '
                       'or tape-level transition. Cyclotron 1000-frame guest and '
                       'pixel equivalence passed; HW16 FPS and sound remain pending. ')
        elif candidate == 'oric-directpack30':
            heading = ('# Oric direct packed ULA 30\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. This unprofiled candidate '
                       'writes 3-bit indexed scanlines directly from the Oric ULA '
                       'renderer, avoiding 240-byte pixel expansion and a separate '
                       'pixel-to-3-bit repack for every changed line. Host pixel '
                       'and machine equivalence passed; real gameplay FPS and '
                       'sound remain pending hardware measurement. ')
        elif candidate == 'oric-gameprofile29':
            heading = ('# Oric game profile 29\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. Diagnostic build of the '
                       'candidate 27 emulator with sampled CPU, keyboard prep, VIA, '
                       'bus, AY and ULA costs. Run Cyclotron in the actual game, '
                       'then collect PERF ORIC and PERF CORE pairs. Profiling may '
                       'lower FPS; this image is not a performance acceptance candidate. ')
        elif candidate == 'oric-rowcache27':
            heading = ('# Oric row cache 27\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. Visible-memory writes '
                       'invalidate affected scanlines instead of redrawing all '
                       '224 lines for every text animation update. Character set '
                       'and mode changes force a full redraw. Cyclotron host '
                       'frame comparison passed; hardware FPS and sound remain '
                       'pending user measurement. ')
        elif candidate == 'oric-fastload26':
            heading = ('# Oric fastload 26\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. Single-block autorun '
                       'machine-code TAPs load directly after BASIC initialization; '
                       'other tapes keep the cassette path. This candidate requires '
                       'user hardware acceptance. ')
        elif candidate == 'oric-targetfps25':
            heading = ('# Oric target FPS 25\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This keeps candidate 24 '
                       'machine logic and guarded boot while adding target_fps to PERF '
                       'ORIC from the active frame cycle count. Hardware behavior '
                       'remains pending user test. ')
        elif candidate == 'oric-xipguard24':
            heading = ('# Oric XIP guard 24\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. Oric-only startup guard '
                       'keeps SDK PSRAM reinitialization and flash restoration in SRAM. '
                       'It uses the same unprofiled machine build as candidate 23. '
                       'Hardware startup, FPS, image and sound remain unverified. ')
        elif candidate == 'oric-release23':
            heading = ('# Oric release 23\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This controlled test '
                       'disables Oric cycle profiling while retaining PERF ORIC wall '
                       'clock, audio underflow, and video counters. Compare the same '
                       'HIRES scene against candidate 22. Hardware performance and '
                       'sound remain unverified. ')
        elif candidate == 'oric-profilesample22':
            heading = ('# Oric profile sample 22\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. The VIA fast/steady '
                       'profile hit counters now sample once per raster line instead '
                       'of writing every machine cycle; UART hit values scale by 64. '
                       'Machine behavior and timing target remain unchanged. Hardware '
                       'FPS, sound, and profile buckets remain unverified. ')
        elif candidate == 'oric-profilephase21':
            heading = ('# Oric profile phase 21\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. Emulator behavior matches '
                       'candidate 19. PERF CORE samples rotate across the 64 cycles of '
                       'a raster line to reduce phase bias. Candidate 20 overflow claim '
                       'was incorrect because profile_cycles already has uint64_t type. '
                       'Hardware bucket accuracy and FPS remain unverified. ')
        elif candidate == 'oric-profilefix20':
            heading = ('# Oric profile fix 20\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. Emulator behavior matches '
                       'candidate 19. An explicit cast was added to PERF CORE scaling; '
                       'the claimed 32-bit overflow was later disproven because the '
                       'source counter was already uint64_t. See HVR-ORIC-026. ')
        elif candidate == 'oric-viabudget19':
            heading = ('# Oric VIA budget 19\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. At 378 MHz a per-raster '
                       'timer budget avoids rechecking the quiet VIA timer and IRQ '
                       'state on nearly every cycle. Host frame comparisons passed; '
                       'hardware FPS and sound remain unverified. ')
        elif candidate == 'oric-vialazy18':
            heading = ('# Oric VIA lazy 18\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. At 378 MHz the core '
                       'defers mathematically quiet VIA timer ticks and flushes at '
                       'register access, timer boundary and raster-line boundary. '
                       'The UART lazy= field estimates the path coverage. Hardware '
                       'FPS and sound remain unverified. ')
        elif candidate == 'oric-inputcache17':
            heading = ('# Oric input cache 17\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This 378 MHz candidate '
                       'updates VIA external input pins when keys, VIA I/O, AY control '
                       'or tape level change, avoiding repeated keyboard matrix and '
                       'pin assembly work on ordinary CPU cycles. Browser diagnostics '
                       'from candidate 16 are removed. Hardware FPS is unverified. ')
        elif candidate == 'oric-browserdiag16':
            heading = ('# Oric browser diagnostics 16\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This is candidate 13 '
                       'at 378 MHz with one-time boot/browser stage logs and browser '
                       'video counters. It does not claim an FPS fix. ')
        elif candidate == 'oric-profile15':
            heading = ('# Oric profile 15\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. At 378 MHz this build '
                       'avoids the tape pulse call while the motor is off and samples '
                       'core timing every 256 cycles. Reported fast/steady hits are '
                       'estimates; core and audio state remain cycle-by-cycle. ')
        elif candidate == 'oric-clock43214':
            heading = ('# Oric clock 432 14\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This diagnostic build '
                       'keeps candidate 13 frame reuse and uses the existing board '
                       'runtime at 432 MHz. Boot, audio and FPS need hardware checks. ')
        elif candidate == 'oric-presentskip13':
            heading = ('# Oric present skip 13\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This build returns to '
                       '378 MHz and reuses the published RGB565 frame when guest video '
                       'is unchanged, while still emulating and producing audio. ')
        elif candidate == 'oric-clock12':
            heading = ('# Oric clock 12\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This diagnostic build '
                       'uses the existing board runtime at 504 MHz, with Flash and PSRAM '
                       'divisors derived from the live clock. ')
        elif candidate == 'oric-viasteady11':
            heading = ('# Oric VIA steady 11\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This build shortcuts '
                       'stable VIA timer cycles and reports steady= hits. ')
        elif candidate == 'oric-cache10':
            heading = ('# Oric cache 10\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This build reuses the '
                       'last rendered HIRES frame while screen memory is unchanged. ')
        elif candidate == 'oric-speed9':
            heading = ('# Oric speed 9\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This diagnostic build '
                       'runs at 378 MHz and skips unchanged AY reads and idle AY ticks. ')
        elif candidate == 'oric-packed8':
            heading = ('# Oric packed 8\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This version keeps a '
                       'packed video frame in PSRAM to preserve SRAM for the HSTX buffer. ')
        elif candidate == 'oric-buffer7':
            heading = ('# Oric buffer 7\n\nLoader installation source: oric.uf2; '
                       'destination: /emu/16/oric.uf2. The standalone image is only '
                       'for BOOTSEL, not the Loader directory. This version defers '
                       'framebuffer acquisition until after emulation and reports acquire= timing. ')
        elif candidate == 'oric-viafast6':
            heading = ('# Oric VIA fast path 6\n\nThe Oric adapter uses an equivalent '
                       'stable-input 6522 tick path, checked against the original on host. '
                       'Capture steady PERF ORIC and PERF CORE lines for hardware timing. ')
        elif candidate == 'oric-diagnostic5':
            heading = ('# Oric diagnostic 5\n\nThe HIRES TAP has a clear text overlay. '
                       'PERF CORE now splits VIA preparation from VIA tick. ')
        elif candidate == 'oric-visual4':
            heading = ('# Oric visual 4\n\nThe included HIRES TAP draws a labeled video chart. '
                       'The firmware retains PERF CORE timing probes. ')
        else:
            heading = ('# Oric diagnostic 3\n\nThis is a timing probe for candidate 2. '
                       'Run the same HIRES TAP and capture consecutive PERF ORIC and PERF CORE lines. ')
        status = ('Hardware FPS improvement is pending measurement.\n\n' if candidate in ('oric-viafast6','oric-buffer7','oric-packed8','oric-speed9','oric-cache10','oric-viasteady11','oric-clock12','oric-presentskip13','oric-clock43214','oric-profile15','oric-inputcache17','oric-vialazy18','oric-viabudget19','oric-profilefix20','oric-profilephase21','oric-profilesample22','oric-release23','oric-xipguard24','oric-rowcache27')
                  else 'No FPS fix is claimed by this image.\n\n')
        (destination/'README.md').write_text(
            heading + status +
            (ROOT/'README.md').read_text(encoding='utf-8'), encoding='utf-8')
    else:
        shutil.copy2(ROOT/'README.md', destination/'README.md')
    shutil.copy2(ROOT.parent/'compat/COPYING', destination/'COPYING')
    shutil.copy2(ROOT/'third_party/chips/LICENSE', destination/'CHIPS-LICENSE')
    (destination/'loader-entry.txt').write_text('oric;oric;Oric-1 / Atmos\n', encoding='utf-8')
    metadata['sourceCommit'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    metadata['sourceDirty'] = bool(subprocess.check_output(['git','status','--porcelain','--','oric'],cwd=ROOT.parent,text=True).strip())
    (destination/'candidate.json').write_text(json.dumps(metadata,indent=2)+'\n')
    files = sorted(p for p in destination.rglob('*') if p.is_file() and p.name != 'SHA256SUMS')
    assert not any(p.suffix.lower()=='.rom' for p in files)
    (destination/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(destination).as_posix()}\n' for p in files))
    print(json.dumps(metadata,indent=2))
    print(destination)


if __name__ == '__main__':
    main()
