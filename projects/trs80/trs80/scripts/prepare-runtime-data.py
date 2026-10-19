#!/usr/bin/env python3
"""Fetch checksum-pinned local test media and preserve archive notices."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2] / "_deps" / "runtime-data"
TIM = "https://www.tim-mann.org/trs80/"
REVISION = "3f31faebde2214214acb74f87f06b836df3914d2"
RETRO = f"https://raw.githubusercontent.com/RetroStack/TRS-80-Model-I-System-ROMs/{REVISION}/1-Option_2x64/"
SOURCES = [
    ("ld1-531.zip", TIM + "ld1-531.zip", "CD426B043292E71C0D97059AD9E59931596B74CD3C51FA27A93B920DDC189D84"),
    ("ld3-531.zip", TIM + "ld3-531.zip", "9D2FE0CC4E8D5F32FBBAFA422E0DF6FFFBB9E9E837EFC6FDB45A0A476F84651B"),
    ("ld4-631.zip", TIM + "ld4-631.zip", "06A093C42BD5C10B431239D52C1BEB1360CC41DD09FA3849ED19FBC0706F1F1D"),
    ("ld4-631.iii", TIM + "ld4-631.iii", "7B9858C68AE7CD66DA9AF9221492C25D72008242F36C546400D09AFE9144F49C"),
    ("Cornsoft.zip", TIM + "Cornsoft.zip", "60FF7A7C603B6CB9B930EA95BB2B703AB78326B3A28867585FBEEB9B13632C2E"),
    ("Lance.zip", TIM + "Lance.zip", "4D4D18EEA77460876FC0FD7F65D1AFCB459979E9270A09ABFE5048F115F84BD1"),
    ("gobbler.zip", TIM + "gobbler.zip", "268627664E9676E168D0235A9890E6514E7D4D3D6AF28BD667A3E1234288CF1D"),
    ("gobbler4.zip", TIM + "gobbler4.zip", "985C68D012EE63457F67EF399A5C4F6FA11FCAE45D2850162077AAA12DC1539C"),
    ("model1_a.bin", RETRO + "rom_a_L2_v13.bin", "1FE45CE666B6B4BDA152ADF929548AEAE17657A1F9079D02DE457AE6A3BDB77F"),
    ("model1_b.bin", RETRO + "rom_b_L2_v13.bin", "A191D73EABC11B6B63F9FE66D47967EFDB36E76FC97739B88D5E94AA74E3418B"),
]

def digest(data):
    return hashlib.sha256(data).hexdigest().upper()

def convert_modela(data):
    memory = bytearray([0xFF] * 0x3800)
    pos = 0
    maximum = 0
    loaded = 0
    while pos < len(data):
        if len(data) - pos < 2:
            raise ValueError("truncated MODELA record")
        kind, count = data[pos:pos + 2]
        pos += 2
        if kind == 1:
            count = (count - 2) % 256 or 256
            if len(data) - pos < count + 2:
                raise ValueError("truncated MODELA data")
            address = int.from_bytes(data[pos:pos + 2], "little")
            pos += 2
            if address + count > len(memory):
                raise ValueError("MODELA data outside BIOS window")
            memory[address:address + count] = data[pos:pos + count]
            maximum = max(maximum, address + count)
            loaded += count
            pos += count
        elif kind == 2:
            if count != 2 or len(data) - pos < 2 or not loaded:
                raise ValueError("invalid MODELA transfer")
            return bytes(memory[:maximum])
        elif kind in (0, 5, 7):
            count = count or 256
            if len(data) - pos < count:
                raise ValueError("truncated MODELA metadata")
            pos += count
        else:
            raise ValueError(f"unsupported MODELA record {kind}")
    raise ValueError("MODELA has no transfer record")

def main():
    for subdir in ("downloads", "bios", "roms", "notices"):
        (ROOT / subdir).mkdir(parents=True, exist_ok=True)
    inventory = []
    for name, url, checksum in SOURCES:
        target = ROOT / "downloads" / name
        if target.exists() and digest(target.read_bytes()) == checksum:
            data = target.read_bytes()
        else:
            with urllib.request.urlopen(url, timeout=30) as response:
                data = response.read(8 * 1024 * 1024 + 1)
            if len(data) > 8 * 1024 * 1024 or digest(data) != checksum:
                raise ValueError(f"download size or hash mismatch: {name}")
            target.write_bytes(data)
        inventory.append({"file": f"downloads/{name}", "url": url,
                          "size": len(data), "sha256": checksum})
        if name.endswith(".zip"):
            with zipfile.ZipFile(target) as archive:
                (ROOT / "notices" / (name + ".txt")).write_bytes(archive.comment)
                for info in archive.infolist():
                    if info.is_dir():
                        continue
                    if Path(info.filename).name != info.filename or "\\" in info.filename or ":" in info.filename:
                        raise ValueError(f"unsafe archive name: {info.filename}")
                    if info.file_size > 8 * 1024 * 1024:
                        raise ValueError("archive member too large")
                    destination = ROOT / "roms" / target.stem / info.filename
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(archive.read(info))
    a = (ROOT / "downloads/model1_a.bin").read_bytes()
    b = (ROOT / "downloads/model1_b.bin").read_bytes()
    if len(a) != 8192 or len(b) != 8192:
        raise ValueError("unexpected Model I padded ROM size")
    (ROOT / "bios/model1.rom").write_bytes(a + b[:4096])
    modela = convert_modela((ROOT / "downloads/ld4-631.iii").read_bytes())
    (ROOT / "bios/model3.rom").write_bytes(modela)
    (ROOT / "bios/model4.rom").write_bytes(modela)
    for path in sorted(ROOT.rglob("*")):
        if path.is_file() and path.name != "inventory.json" and "downloads" not in path.parts:
            data = path.read_bytes()
            inventory.append({"file": path.relative_to(ROOT).as_posix(),
                              "size": len(data), "sha256": digest(data)})
    (ROOT / "inventory.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
    print(f"LOCAL MEDIA PREPARED: {ROOT}")
    print(f"MODELA/III converted: {len(modela)} bytes")
    print("Hardware readiness is pending firmware and runtime verification.")

if __name__ == "__main__":
    main()
