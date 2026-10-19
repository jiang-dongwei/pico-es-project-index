#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check compiled HW16 transition placement, not physical flash reliability."""
import argparse
import re
import subprocess


def check(dump):
    functions = {}
    current = None
    for line in dump.splitlines():
        match = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if match:
            current = match[2]
            functions[current] = (int(match[1], 16), [])
        elif current:
            functions[current][1].append(line)

    def find(fragment):
        matches = [n for n in functions if fragment in n and "veneer" not in n]
        if len(matches) != 1:
            raise ValueError(f"Expected one {fragment}: {matches}")
        return matches[0]

    names = [find(n) for n in (
        "__wrap_psram_reinitialize", "enableFlashQuad6BForClock",
        "configQspiPads", "flashCommandDrained", "drainPsramCommandRx",
        "drain_startup_rx", "psram_initialize_internal")]
    names.append("psram_reinitialize")
    for name in names:
        addr, lines = functions[name]
        if not 0x20000000 <= addr < 0x20082000:
            raise ValueError(f"Transition routine in XIP: {name} {addr:08x}")
        # Allow SDK lookups/clock_get_hz BEFORE the transition, never after.
        if name == "psram_reinitialize":
            boundary = "<flash_start_xip>"
        elif name == "__wrap_psram_reinitialize":
            boundary = "<psram_reinitialize>"
        else:
            boundary = None
        active = boundary is None
        found = active
        for line in lines:
            if boundary and boundary in line:
                active = found = True
            if not active:
                continue
            call = re.search(r"\b(?:bl|blx|b\.w)\s+([0-9a-f]+)\s+<([^>]+)>", line)
            if call:
                dest = int(call[1], 16)
                if 0x10000000 <= dest < 0x20000000:
                    raise ValueError(f"Flash branch in {name}: {line.strip()}")
                target = call[2]
                if "veneer" in target and target in functions:
                    for insn in functions[target][1]:
                        word = re.search(r"\.word\s+0x([0-9a-f]+)", insn)
                        if word and 0x10000000 <= int(word[1], 16) < 0x20000000:
                            raise ValueError(f"Flash veneer in {name}: {target}")
            literal = re.search(r"\.word\s+0x([0-9a-f]+)", line)
            if literal and 0x10000000 <= int(literal[1], 16) < 0x14000000:
                raise ValueError(f"Potential flash literal in {name}: {line.strip()}")
        if not found:
            raise ValueError(f"Missing transition boundary {boundary} in {name}")

    init_name = find("9initPsramEv")
    if "__wrap_psram_reinitialize" not in "\n".join(functions[init_name][1]):
        raise ValueError("Frens initPsram does not call guarded reinitialize")
    print("XIP transition ELF gate PASS: SDK store, wrapper, restore and FIFO helpers in SRAM")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--objdump", required=True)
    parser.add_argument("--elf", required=True)
    args = parser.parse_args()
    try:
        check(subprocess.check_output([args.objdump, "-d", args.elf], text=True))
    except (ValueError, KeyError, subprocess.CalledProcessError) as error:
        raise SystemExit(f"XIP transition ELF gate FAIL: {error}")
