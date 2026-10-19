# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root = Path(__file__).resolve().parents[1]
out = root / 'assets/loader/ti83.png'
out.parent.mkdir(parents=True, exist_ok=True)
im = Image.new('RGB', (320, 240), '#132d36')
d = ImageDraw.Draw(im)
font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 24)
small = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 12)
d.text((16, 17), 'TI-83 / 84', font=font, fill='#def4d1')
d.text((16, 51), 'CLASSIC Z80', font=small, fill='#72c6b3')
d.rounded_rectangle((197, 13, 302, 220), radius=12, fill='#303743', outline='#94a3a7', width=2)
d.rectangle((209, 30, 290, 84), fill='#afc995')
for x in range(76):
    y = 65-int(17*((x-38)/38)**2)
    d.point((212+x,y), fill='#193028')
d.line((211,69,288,69), fill='#193028')
d.line((249,35,249,79), fill='#193028')
for y in range(7):
    for x in range(5):
        d.rounded_rectangle((210+x*16,98+y*15,221+x*16,108+y*15),radius=2,
                           fill='#66b8b2' if y==0 else '#ced4d8')
d.text((18,100),'96 x 64 LCD',font=small,fill='#e2ece9')
d.text((18,126),'FULL KEYPAD',font=small,fill='#e2ece9')
d.text((18,152),'RP2350B',font=small,fill='#e2ece9')
d.text((18,202),'RETROPICOES',font=small,fill='#72c6b3')
im.save(out)
print(out)
