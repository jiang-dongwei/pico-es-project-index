# Reproducible external dependency pins for RetroPicoES v2.1.

set(RETROPICOES_PICO_SHARED_REPOSITORY
    "https://github.com/fhoedemakers/pico_shared.git")
set(RETROPICOES_PICO_SHARED_COMMIT
    "3e19ce0e89bdc55d4807217499d0d54b508bedbb")
set(RETROPICOES_PICO_SHARED_PATCH_SHA256
    "4502878e4d58b99f640c810af02b0c121e727ebfd5d04b1792ff9ab0d20ac69a")

# hardware_psram first ships in the stable 2.3.0 SDK.  The handoff calls its
# pre-release checkout "2.2.x pico-sdk-dev"; pin the released equivalent.
set(RETROPICOES_PICO_SDK_REPOSITORY
    "https://github.com/raspberrypi/pico-sdk.git")
set(RETROPICOES_PICO_SDK_TAG "2.3.0")
set(RETROPICOES_PICO_SDK_COMMIT
    "98a542c1a62fb549ffb5d66a3e5892b06276b670")

# Match the dependency selected by TinyUSB in Pico SDK 2.3.0.
set(RETROPICOES_PICO_PIO_USB_REPOSITORY
    "https://github.com/sekigon-gonnoc/Pico-PIO-USB.git")
set(RETROPICOES_PICO_PIO_USB_COMMIT
    "fe9133fc513b82cc3dc62c67cb51f2339cf29ef7")
set(RETROPICOES_PICO_PIO_USB_PATCH_SHA256
    "c6097c792c0426f3caa4ac1402d4388dd7131be93d119c463f6315513ce7c5d2")

# pico-infonesPlus uses fhoedemakers' compatibility fork as a submodule.
set(RETROPICOES_TUSB_XINPUT_REPOSITORY
    "https://github.com/fhoedemakers/tusb_xinput.git")
set(RETROPICOES_TUSB_XINPUT_COMMIT
    "915b21ac3d86d279d4e2b52384f1910ac633a607")
set(RETROPICOES_TUSB_XINPUT_PATCH_SHA256
    "2d4e7c0bc5b8f4001ffdcc4b0ac871100940c8743c953a0b9f7417e2338f41a3")

# ---------------------------------------------------------------------------
# S17 noiz2sa port pins.
# ---------------------------------------------------------------------------

# Engine: M-HT/noiz2sa lineage of Kenta Cho's noiz2sa 0.52.
# Tip commit as of 2026-09-16 (0.52 import with SDL2 cleanup); the SDL API
# surface outside screen.c/soundmanager.c is shimmed by the port include
# path, and screen.c/soundmanager.c are replaced by port implementations.
set(NOIZ2SA_REPOSITORY "https://github.com/M-HT/noiz2sa.git")
set(NOIZ2SA_COMMIT "2253d9cee2026311a5af9e4eeba66707263b5d9e")
set(NOIZ2SA_PATCH
    "${CMAKE_CURRENT_LIST_DIR}/../patches/retropicoes-v2.1/noiz2sa-engine-main.patch")
set(NOIZ2SA_PATCH_SHA256
    "143ac30aead02b787a2de51c410c1e56d999bb09f5b8e476c9001d2b13fea040")

# libBulletML 0.0.6 tarball from the upstream author's site.
set(NOIZ2SA_BULLETML_URL
    "http://shinh.skr.jp/libbulletml/libbulletml-0.0.6.tar.bz2")
set(NOIZ2SA_BULLETML_SHA256
    "7c37f3d2d52825417c5de716f89bea4b71156371e698e2579daf7921df07aa79")

# stb_vorbis.c v1.22 (public domain), pinned to the nothings/stb commit that
# last touched the file. Hash is of the raw (LF) file.
set(NOIZ2SA_STB_VORBIS_URL
    "https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_vorbis.c")
set(NOIZ2SA_STB_VORBIS_SHA256
    "4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636")
