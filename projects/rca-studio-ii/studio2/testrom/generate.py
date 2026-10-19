# SPDX-License-Identifier: GPL-3.0-or-later
"""Original 1802 boot diagnostic. Not the RCA BIOS. No commercial data."""
from pathlib import Path
import argparse

def image(sound=False):
    rom=bytearray([0xff]*2048)
    code=[0xf8,0,0xb4,0xf8,7,0xa4,0xd4, # main PC R4, R0 reserved for DMA
          0xf8,0x01,0xb1,0xf8,0x00,0xa1, # IRQ R1=0100
          0xf8,0x09,0xb2,0xf8,0xff,0xa2, # stack R2=09ff
          0xf8,0x08,0xb3,0xf8,0x00,0xa3, # fill R3=0800
          0xf8,0xaa,0x53,0x13,0x83,0x3a,0x19, # stripe first 256 bytes
          0xf8,0x55,0x53,0x13,0x83,0x3a,0x20, # stripe next 256 bytes
          0xe2,0x69] # INP1, enable Pixie
    if sound:code += [0x7b]
    # Latch key 1. EF3/EF4 gate beep and a pattern byte.
    loop=len(code)
    code += [0xf8,1,0x52,0x62,0x22,0x36,0,0x37,0,0x7b if sound else 0x7a,0x30,loop]
    beep=len(code)
    code += [0x7b,0x30,loop]
    code[loop+6]=beep;code[loop+8]=beep
    rom[:len(code)]=bytes(code)
    irq=[0x22,0x78,0xf8,8,0xb0,0xf8,0,0xa0, # save T, R0=0800
         0x34,0x08, # B1 wait for display start
         0x3c,0x0a, # BN1 wait for display end
         0x70] # RET through R2
    rom[256:256+len(irq)]=bytes(irq)
    return bytes(rom)
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    for name,sound in [('PicoES_Studio2_Input.s2d',False),('PicoES_Studio2_Tone.s2d',True)]:
        (a.output/name).write_bytes(image(sound))
