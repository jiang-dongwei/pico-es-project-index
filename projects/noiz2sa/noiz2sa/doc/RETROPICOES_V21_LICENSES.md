# RetroPicoES v2.1 noiz2sa source and data inventory

The Pico_ES project is GPL-3.0, but every imported component retains its own
terms. This inventory does not replace those notices.

## Engine and direct dependencies

| Component | Fixed source | Terms/evidence |
| --- | --- | --- |
| noiz2sa engine | `M-HT/noiz2sa` commit `2253d9cee2026311a5af9e4eeba66707263b5d9e` (lineage of Kenta Cho's noiz2sa 0.52) | BSD-2-Clause. The license text is in the upstream `readme.txt`/`readme_e.txt` ("Redistribution and use in source and binary forms..."); source headers carry "Copyright 2002 Kenta Cho. All rights reserved." |
| libBulletML | 0.0.6 tarball from `http://shinh.skr.jp/libbulletml/libbulletml-0.0.6.tar.bz2`, SHA-256 `7c37f3d2d52825417c5de716f89bea4b71156371e698e2579daf7921df07aa79` | Modified BSD ("修正BSDライセンス") license statement in the tarball `README`: Copyright (c) 2003, shinichiro.h, three-clause redistribution conditions. |
| TinyXML (vendored by libBulletML `bulletml/src/tinyxml/`) | same tarball | zlib-style license, Copyright (c) 2000 Lee Thomason (www.grinninglizard.com), in `tinyxml.h`. |
| Bison 1.28 parser skeleton (embedded in libBulletML `bulletml/src/calc.cpp`) | same tarball | GPL-2 skeleton with the standard Bison special exception: "when this file is copied by Bison into a Bison output file, you may use that output file without restriction." |
| stb_vorbis | v1.22 from `nothings/stb` commit `2c980bb59875b0d32144a71867fbdebb2f77cd20`, file SHA-256 `4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636` | Public domain / MIT dual license, stated in the file header and license block. |
| `fhoedemakers/pico_shared` | commit `3e19ce0e89bdc55d4807217499d0d54b508bedbb` plus checked HW16 patch | GPL-3.0, dependency `LICENSE` (S17-D consumer; local copy pinned unchanged from the xrick port). |
| Raspberry Pi Pico SDK | 2.3.0 commit `98a542c1a62fb549ffb5d66a3e5892b06276b670` | BSD-3-Clause, SDK `LICENSE.TXT`. |
| compat v1 | this workspace `compat/` v1.0.0, `manifest.sha256` | GPL-3.0-or-later, module `COPYING`; consumed unmodified. |

Not compiled: libBulletML's `yggdrasil` (TrickLib, custom terms) and `xerces`
parser variants are extracted with the tarball but excluded from every build
target; only the tinyxml parser path is used.

## Game data boundary

The engine expects engine-relative data directories supplied by the user:

```text
/data/noiz2sa/images/   title BMP sprites (from the official free archive)
/data/noiz2sa/sounds/   7 Ogg BGM tracks and 7 WAV effects
/data/noiz2sa/zako/     BulletML small-enemy patterns
/data/noiz2sa/middle/   BulletML middle-enemy patterns
/data/noiz2sa/boss/     BulletML boss patterns
```

noiz2sa is freeware distributed by Kenta Cho (ABA Games) with the BSD-2
source license above, but this port still follows the project data red line:

- the build and the SD installer never read from or package `/data/noiz2sa/`;
- users obtain the official free archive themselves and place it on the SD
  card; the installer only documents the expected layout and preflight file
  counts at the RUNTIME-DATA-GATE;
- `/SAVES/noiz2sa/noiz2sa.prf` (preference/high-score file) is user state and
  must not be committed;
- no game data file is committed to Git by this port.
