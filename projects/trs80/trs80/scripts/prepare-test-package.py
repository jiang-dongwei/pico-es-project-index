"""Stage existing downloaded media locally; never write an SD drive."""
from pathlib import Path
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
source = root.parent / "_deps" / "runtime-data"
target = root / "build-sd-test-package"
files = []

def copy(src, relative):
    dest = target / relative
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(src, dest)
    files.append({"path": relative, "sha256": hashlib.sha256(dest.read_bytes()).hexdigest()})

for model in (1, 3, 4):
    copy(source / "bios" / f"model{model}.rom", f"data/trs80/bios/model{model}.rom")
for src, dst in (
    ("ld1-531/ld1-531.dsk", "Model_I/LDOS_531.JV1"),
    ("ld3-531/ld3-531.dsk", "Model_III/LDOS_531.JV3"),
    ("ld4-631/ld4-631.dsk", "Model_4/LSDOS_631.JV3"),
    ("Cornsoft/FROGGER.CMD", "Model_I/FROGGER.CMD"),
    ("Cornsoft/CASTLE.CMD", "Model_I/CASTLE.CMD"),
    ("gobbler/gobbler.dsk", "Model_I/GOBBLER.JV1"),
    ("gobbler4/gobbler4.dsk", "Model_4/GOBBLER.JV3"),
):
    copy(source / "roms" / src, "roms/TRS80/" + dst)
for notice in sorted((source / "notices").glob("*")):
    if notice.is_file():
        copy(notice, "notices/" + notice.name)
copy(source / "inventory.json", "notices/source-inventory.json")
(target / "manifest.json").write_text(json.dumps(files, indent=2) + "\n", encoding="utf-8")
(target / "README.txt").write_text(
    "Local test media only. Preserve all notices; see trs80/doc/RUNTIME_MEDIA.md.\n"
    "Model_I, Model_III and Model_4 folders automatically select their matching machine.\n"
    "JV1/JV3 mounts are read-only. No SD card was modified by this script.\n",
    encoding="utf-8",
)
print(target)
