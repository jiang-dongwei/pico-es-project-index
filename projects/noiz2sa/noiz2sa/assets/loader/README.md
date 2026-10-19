# noiz2sa Loader artwork brief

The S17 Loader entry requires an original `noiz2sa.png` at this path before
the SD installer can create a complete delivery.  No bitmap is committed yet.

## Required asset

- PNG, exactly 320×240 pixels, opaque RGB/RGBA.
- File name: `noiz2sa.png`.
- Text: `NOIZ2SA` and a small `RetroPicoES` subtitle only.
- Composition: a clean abstract arcade field of luminous geometric particles
  converging toward a small central player craft silhouette; leave the lower
  edge visually quiet for Loader captions.
- Palette: deep navy/black background, cyan, magenta, and warm yellow accent.

## Boundaries

This is new port artwork, not a reproduction or crop of ABA Games title
sprites, screenshots, box art, or game data.  Do not take it from
`/data/noiz2sa/` and do not package user data into the Loader asset.

The installer deliberately fails if this PNG is absent.  That prevents an
incomplete SD deployment from being described as a finished S17-E delivery.
