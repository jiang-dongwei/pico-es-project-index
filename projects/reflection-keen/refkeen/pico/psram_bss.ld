/* Large ReflectionHLE emulated video memories are accessed only after
 * compat_init has brought up RP2350 PSRAM. Keep them out of scarce SRAM and
 * explicitly clear them from the video backend after PSRAM initialization. */
SECTIONS
{
    /* Keep the lower 6 MiB available to this port's isolated lwmem heap. */
    .refkeen_psram_bss 0x11600000 (INFO) :
    {
        __refkeen_psram_bss_start = .;
        *(.bss.g_sdlVidMem)
        *(.bss.g_sdlHostScrMem)
        *(.bss.g_sdlHostScrMemCache)
        *(.bss.g_sdlEGACurrBGRAPalette)
        *(.bss.g_sdlEGACurrBGRAPaletteCache)
        *(.bss.g_be_emulatedMemSpace)
        *(.bss.FRAMEBUFFER)
        *(.bss.g_oplChip)
        *(.bss.g_be_gameinstallations)
        *(.bss.g_ingame_altcontrol*)
        *(.bss.g_beStControllerMapping*)
        *(.bss.g_picoAltControlSchemeDefaultMapping)
        *(.bss.grsegs)
        *(.bss.objarray)
        *(.bss.g_be_rootPaths)
        *(.bss.grneeded)
        *(.bss.spritearray)
        *(.bss.databuffer)
        /* Game-state BSS is not touched until after compat_init/PSRAM init. */
        *kd_*.c.obj(.bss*)
        *id_*.c.obj(.bss*)
        *lzhuf.c.obj(.bss*)
        *loadscn2.c.obj(.bss*)
        *unlzexe.c.obj(.bss*)
        *be_*.c.obj(.bss*)
        __refkeen_psram_bss_end = .;
    }
}
INSERT BEFORE .bss;
