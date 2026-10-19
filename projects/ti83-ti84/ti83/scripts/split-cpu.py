#!/usr/bin/env python3
"""Generate a split dispatcher without changing the hash-locked TilEm sources."""
from pathlib import Path
import re
import sys

upstream, output = map(Path, sys.argv[1:])
output.mkdir(parents=True, exist_ok=True)
source = (upstream / 'z80.c').read_text()
source = '#include "cpu_profile.h"\n#include "fast_memory.h"\n' + source
begin=source.index('static inline byte z80_readb(TilemCalc* calc, dword addr)\n')
end=source.index('static inline dword z80_readw(',begin)
operand=source[begin:end].replace('z80_readb(','z80_readpc(').replace('(*calc->hw.z80_rdmem)(calc, addr)','ti83_fast_read_operand(calc, addr)')
source=source[:end]+operand+source[end:]
needle='#define readb(aaa)        z80_readb(calc, aaa)'
assert source.count(needle)==1
source=source.replace(needle,needle+'\n#define readpc(aaa)       z80_readpc(calc, aaa)')
needle='\tdword v;\n\taddr &= 0xffff;'
assert source.count(needle)==1
source=source.replace(needle,'\tdword v;\n\tif (ti83_inline_read_word(calc,addr,&v)) return v;\n\taddr &= 0xffff;')
needle='static inline void z80_writew(TilemCalc* calc, dword addr, word value)\n{'
assert source.count(needle)==1
source=source.replace(needle,needle+'\n\tif (ti83_inline_write_word(calc,addr,value)) return;')
needle = '\t\top = (*calc->hw.z80_rdmem_m1)(calc, PC);'
assert source.count(needle) == 1
source = source.replace(needle, '\t\tti83_profile_begin();\n'+needle+'\n\t\tti83_profile_fetch(op);')
source=source.replace(needle,'#ifdef TI83_CPU_PROFILE\n\t\t++ti83_cpu_pc[PC];\n#endif\n'+needle)
assert source.count(needle)==1
source=source.replace(needle,'\t\top = ti83_fast_fetch_opcode(calc, PC);')
source='#ifdef TI83_CPU_PROFILE\nunsigned ti83_cpu_pc[65536];\n#endif\n'+source
needle = '\t\top = z80_execute_opcode(calc, op);'
assert source.count(needle) == 1
source = source.replace(needle, needle+'\n\t\tti83_profile_dispatch();')
needle = '\t\tcheck_timers(calc);\n\n\t\tif (z80->interrupts'
assert source.count(needle) == 1
source = source.replace(needle, '\t\tcheck_timers(calc);\n\t\tti83_profile_timer();\n\n\t\tif (z80->interrupts')
needle = '\n\t}\n}\n\nstatic void tmr_stop('
assert source.count(needle) == 1
source = source.replace(needle, '\n\t\tti83_profile_end();'+needle)
commands = (upstream / 'z80cmds.h').read_text()
declaration = 'static const byte parity_table[256] ='
assert commands.count(declaration) == 1
commands = commands.replace(declaration,
    'static const byte parity_table[256] __attribute__((section(".time_critical.ti83_parity"),aligned(4))) =')
(output / 'z80cmds_hot.h').write_text(commands)
source = source.replace('#include "z80cmds.h"', '#include "z80cmds_hot.h"')
for call, guard in (
    ('check_breakpoints(calc, list_l, addr);', 'list_l'),
    ('check_breakpoints(calc, z80->breakpoint_op, op);', 'z80->breakpoint_op'),
    ('check_breakpoints(calc, calc->z80.breakpoint_pr, addr);', 'calc->z80.breakpoint_pr'),
    ('check_breakpoints(calc, calc->z80.breakpoint_pw, addr);', 'calc->z80.breakpoint_pw'),
):
    assert source.count(call) == 1
    source = source.replace(call, f'if ({guard}) {call}')
start = source.index('static dword z80_execute_opcode(')
end = source.index('\nstatic void z80_execute(', start)
body = source[start:end]
locals_start = body.index('\n\tbyte tmp1;')
locals_end = body.index('\n opcode_main:')
locals_text = body[locals_start:locals_end]
main = (upstream / 'z80main.h').read_text()
assert main.count('readb(PC++)')==24
main=main.replace('readb(PC++)','readpc(PC++)')
for prefix in ('cb', 'ed', 'dd', 'fd'):
    old = f'goto opcode_{prefix};'
    assert main.count(old) == 1
    main = main.replace(old, f'{{*next=0x{prefix};return op;}}')
(output / 'z80main_split.h').write_text(main)
main_ops = {0x00,0x01,0x06,0x10,0x18,0x20,0x21,0x23,0x25,0x28,0x2b,
            0x30,0x31,0x32,0x3a,0x3e,0x47,0x4f,0x57,0x5f,0x67,0x6f,
            0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0xa7,0xaf,0xb0,
            0xb7,0xc0,0xc1,0xc3,0xc5,0xc8,0xc9,0xcd,0xd1,0xd3,0xd5,
            0xdb,0xe1,0xe5,0xe6,0xf1,0xf3,0xf5,0xfb,0xfe}
labels = list(re.finditer(r'^ case 0x([0-9A-F]{2}):', main, re.M))
assert len(labels) == 256
selected = []
for i, label in enumerate(labels):
    if int(label[1],16) in main_ops:
        end_block = labels[i+1].start() if i+1<len(labels) else main.rindex('\n }')
        block = main[label.start():end_block]
        assert 'break;' in block and 'goto ' not in block and '*next' not in block
        selected.append(block)
assert len(selected) == len(main_ops)
(output / 'z80main_hot.h').write_text(main[:main.index('switch (op) {')]+
    'switch (op) {\n'+''.join(selected)+
    ' default: return z80_execute_opcode(calc,op);\n }\n')
hot_ops = {
    'ed': (0x42,0x52,0x5b,0x62,0xa3,0xb0,0xb1,0xb8),
    'cb': (0x1b,0x72,0x77,0x7a,0xb2),
    'ddfd': (0x09,0x23,0x77,0x7e,0xb6,0xe1,0xe5),
}
for family, ops in hot_ops.items():
    header = (upstream / f'z80{family}.h').read_text()
    switch = header.index('switch (op) {')
    labels = list(re.finditer(r'^ (?:case 0x([0-9A-F]{2})|default):', header, re.M))
    selected = []
    for i, label in enumerate(labels[:-1]):
        if int(label[1],16) in ops:
            block = header[label.start():labels[i+1].start()]
            assert 'break;' in block and 'goto ' not in block
            selected.append(block)
    assert len(selected) == len(ops)
    tail = header[header.index('\n }',labels[-1].start())+3:]
    (output / f'z80{family}_hot.h').write_text(
        header[:switch]+'switch (op) {\n'+''.join(selected)+' default: return 0;\n }'+tail)
prefix_body = body[body.index('\n opcode_cb:'):body.rindex('\n}')]
replacement = '''
#ifdef TI83_CPU_PROFILE
unsigned ti83_cpu_profile[5][256];
unsigned ti83_cpu_main_hot,ti83_cpu_main_cold;
#endif
static dword z80_main_split(TilemCalc* calc, byte op, int* next)
{''' + locals_text + '''
#ifdef TI83_CPU_PROFILE
    ++ti83_cpu_main_cold;
#endif
#include "z80main_split.h"
    return op;
}
static int z80_hot_prefix(TilemCalc* calc, byte op, int mode, dword* result)
{''' + locals_text + '''
    switch(mode) {
    case 0xcb: {
#include "z80cb_hot.h"
        *result=op|0xcb00;return 1;
    }
    case 0xed: {
#include "z80ed_hot.h"
        *result=op|0xed00;return 1;
    }
    case 0xdd: {
#define PREFIX_DD
#include "z80ddfd_hot.h"
#undef PREFIX_DD
        *result=op|0xdd00;return 1;
    }
    default: return 0;
    }
}
static dword z80_prefix_split(TilemCalc* calc, byte op, int mode, int* next)
{''' + locals_text + '''
    switch(mode) {
    case 0xcb: goto opcode_cb;
    case 0xed: goto opcode_ed;
    case 0xdd: goto opcode_dd;
    case 0xfd: goto opcode_fd;
    default: return op;
    }
opcode_main:
    *next=0;return op;
''' + prefix_body + '''
}
static dword z80_execute_opcode(TilemCalc* calc, byte op)
{
    int mode,next;
    dword result=op;
opcode_main_split:
    mode=-1;
#ifdef TI83_CPU_PROFILE
    ++ti83_cpu_profile[0][(byte)result];
#endif
    result=z80_main_split(calc,(byte)result,&mode);
    while(mode>=0) {
        if(!mode) goto opcode_main_split;
        next=-1;
#ifdef TI83_CPU_PROFILE
        unsigned category=mode==0xcb?1:mode==0xed?2:mode==0xdd?3:mode==0xfd?4:0;
        byte sample=(byte)result;
        ++ti83_cpu_profile[category][sample];
#endif
        if(!z80_hot_prefix(calc,(byte)result,mode,&result))
            result=z80_prefix_split(calc,(byte)result,mode,&next);
        mode=next;
    }
    return result;
}
static inline dword z80_execute_hot(TilemCalc* calc, byte op)
{''' + locals_text + '''
#include "z80main_hot.h"
#ifdef TI83_CPU_PROFILE
    ++ti83_cpu_main_hot;
    ++ti83_cpu_profile[0][op];
#endif
    return op;
}
'''
generated = source[:start] + replacement + source[end:]
needle = '\t\top = z80_execute_opcode(calc, op);'
assert generated.count(needle)==1
generated=generated.replace(needle,'\t\top = z80_execute_hot(calc, op);')
needle = 'static inline void check_timers(TilemCalc* calc)\n'
assert generated.count(needle)==1
generated=generated.replace(needle,'static void check_timers_due(TilemCalc* calc)\n')
at=generated.index('\nstatic inline void check_breakpoints(')
guard='''
static inline void check_timers(TilemCalc* calc)
{
    TilemZ80* z=&calc->z80;
    if ((z->timer_cpu && z->clock-z->timers[z->timer_cpu].count<10000u) ||
        (z->timer_rt && z->clock-z->timers[z->timer_rt].count<10000u))
        check_timers_due(calc);
}
'''
generated=generated[:at]+guard+generated[at:]
(output / 'z80_split.c').write_text(generated)
names = sorted(set(re.findall(r'\b(tilem_z80_\w+)\s*\(', source)))
reference = ''.join(f'#define {n} ti83_reference_{n}\n' for n in names)
reference += f'#include "{(upstream / "z80.c").as_posix()}"\n'
(output / 'z80_reference.c').write_text(reference)
keypad = (upstream / 'keypad.c').read_text()
fast_keypad = keypad.replace('#include "scancodes.h"', '#include "scancodes.h"\n#include <stdint.h>\n#include <string.h>')
needle = '\tkeys = 0;\n'
assert fast_keypad.count(needle) == 1
fast_keypad = fast_keypad.replace(needle, '''\tuint32_t rows0, rows1;
\tmemcpy(&rows0, calc->keypad.keysdown, sizeof(rows0));
\tmemcpy(&rows1, calc->keypad.keysdown + 4, sizeof(rows1));
\tif (!(rows0 | rows1)) return 0xff;
''' + needle)
needle = '\n\tdo {\n\t\told = keys;'
assert fast_keypad.count(needle) == 1
fast_keypad = fast_keypad.replace(needle, '\n\tif (!keys) return 0xff;\n' + needle)
(output / 'keypad_fast.c').write_text(fast_keypad)
names = sorted(set(re.findall(r'\b(tilem_keypad_\w+)\s*\(', keypad)))
reference = ''.join(f'#define {n} ti83_reference_{n}\n' for n in names)
reference += f'#include "{(upstream / "keypad.c").as_posix()}"\n'
(output / 'keypad_reference.c').write_text(reference)
