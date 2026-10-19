# Sources and licenses

* Calculator core: TilEm 2.0, Benjamin Moody, Solignac Julien and other authors
  named in each file. `upstream/emu/` is copied unchanged from
  https://deb.debian.org/debian/pool/main/t/tilem/tilem_2.0.orig.tar.bz2.
  Archive SHA-256: `4e752f63477640d72fd8b48ca5acd7a7590f3b29005d603cd59a687a2fce8576`.
  Emulation sources carry LGPL-2.1-or-later; preserve all original headers.
  `upstream/COPYING.LESSER` contains LGPL 2.1; distribution root `COPYING`
  is retained as well. Unlinked utilities/model sources are retained unchanged.
* Adapter/application: GPL-3.0-or-later. Browser integration, Settings adapter,
  build/installer scripts derive from the project's committed Channel F port
  at `bc8b7b6`, with calculator-specific behavior implemented here. No
  uncommitted changes from that task were copied.
* Shared frontend, compat and HW16 backend: project sources at the task
  baseline `7330ed6`, used without modifications. Existing upstream dependency
  licenses continue to apply.
* Cover: original code-generated calculator illustration; GPL-3.0-or-later.
  Windows Consolas is used locally for rasterization, not distributed as a font.
* Tests: original synthetic Z80 bytes and fabricated inputs, GPL-3.0-or-later.
  No TI operating system, game ROM, personal save or user data is distributed.

TilEm project: https://sourceforge.net/projects/tilem/.
CE calculators are intentionally excluded; the eZ80 CEmu project describes
that distinct machine family at https://github.com/CE-Programming/CEmu.
