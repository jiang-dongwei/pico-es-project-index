<#
    Measured facts about the pinned upstream tree, in one place.

    Two gates read this file and they bucket the SAME translation-unit set:

        Test-PlatformBoundary.ps1   compiles the TUs WITHOUT the shim and
                                    reports what upstream still needs from
                                    the port (the work list).
        Test-ShimSurface.ps1        compiles them WITH the shim and reports
                                    whether the shim is sufficient (the
                                    measure of done).

    Duplicating these lists would let the two scripts drift and report two
    different numbers for one fact, which is worse than reporting no number.
    Edit here only.

    All values are measured at upstream tag release-20260606 (74f07ef), not
    assumed. Two entries in the first draft of this list were wrong, which is
    the reason the boundary gate exists at all:

      * be_video.c IS a platform TU. It owns g_sdlTexture, calls
        BEL_ST_LockTexture and selects the render driver -- it is not the
        neutral core its name suggests.
      * be_events_sdl.c is NOT a direct SDL3 includer. It reaches SDL through
        its own header, be_events_sdl.h.
#>
@{
    # .c files that include real SDL3 directly, and that the port replaces
    # one-for-one with hardware-facing implementations.
    PlatformTus = @(
        'src/backend/audio/be_audio_sdl.c'
        'src/backend/cfg/be_cfg_sdl.c'
        'src/backend/filesystem/be_filesystem_app_paths.c'
        'src/backend/input/be_input_sdl.c'
        'src/backend/startup/be_startup_sdl.c'
        'src/backend/video/be_video.c'
        'src/backend/video/be_video_sdl.c'
    )

    # Headers allowed to include SDL3: they declare types and hooks, they do
    # not link any code the engine calls.
    AllowedSdlHeaders = @(
        'src/backend/events/be_events_sdl.h'
        'src/backend/input/be_input_sdl.h'
        'src/backend/video/be_video_sdl.h'
        'src/backend/threading/be_atomic_sdl.h'
        'src/backend/threading/be_mutex_sdl.h'
    )

    # Outside src/backend on purpose: the program entry point and the platform
    # layer's private header. Both are port responsibilities.
    AllowedOutsideBackend = @(
        'src/actual_main.c'
        'src/be_st_sdl_private.h'
    )

    # The launcher is disabled for this port (REFKEEN_ENABLE_LAUNCHER=OFF).
    LauncherPrefix = 'src/backend/launcher/'

    # TUs the port deliberately does not compile even though they are NOT
    # platform TUs. Each entry is a scope decision, and the consequence of the
    # decision is that whatever the TU defined has to come from the port
    # instead.
    #
    # Together with PlatformTus, EventSource.ReplacedTu and
    # FilesystemSeam.ExcludedTus, this is the whole "not built" set.
    # Test-EventSourceContract.ps1 derives that set from exactly these facts, so
    # that the ownership of a frozen symbol is *measured* against the record
    # instead of being asserted by it.
    #
    # Both entries are the altcontrol decision of platform-boundary.md 8.2
    # (2026-09-23, verdict "stub"). That document names kdreams/altcontroller.c
    # for the RAM reason (90,624 B of writable .data) and leaves the exact stub
    # surface to S20-C. be_input_controller_mappings.c is the backend half of
    # the same subsystem -- the mapping menu, the on-screen controls and the
    # gamecontrollerdb loader, and the BEL_ST_AltControlScheme_* entry points
    # that be_video_ui.c calls -- so the same decision covers it.
    #
    # Implementing the consequence is C12. If C12 concludes that keeping one of
    # these is cheaper than stubbing what it exports, then this fact changes and
    # the gate is re-run: the check follows the record, it does not create it.
    PortExcludedTus = @{
        'src/kdreams/altcontroller.c' =
            'altcontrol mapping tables: 90,624 B of writable .data (platform-boundary.md 8.2)'
        'src/backend/input/be_input_controller_mappings.c' =
            'altcontrol backend: mapping menu, on-screen controls, gamecontrollerdb (same decision)'
    }

    # The platform layer's private surface. Small on purpose: this is the seam.
    BelSt = @(
        'BEL_ST_CPUPauseInstruction'
        'BEL_ST_GetTicksMS'
        'BEL_ST_GetTicksNS'
        'BEL_ST_SleepMS'
    )

    # The SDL_-prefixed names id_sd.h actually declares. These belong to id
    # Software's own sound library (DOS "Sound Driver Library"); the port must
    # not rename them to avoid a collision that does not exist.
    IdSdSymbols = @(
        'SDL_ALPlaySound'
        'SDL_ALStopSound'
        'SDL_PCPlaySound'
        'SDL_PCStopSound'
        'SDL_SBPlaySample'
        'SDL_SBStopSample'
        'SDL_SSPlaySample'
        'SDL_SSStopSample'
    )

    # TUs the SDL3 shim deliberately does NOT cover. Two different kinds live
    # here, and conflating them was the source of a wrong record (see below).
    #
    # Test-ShimSurface.ps1 treats exactly these as "known, not a regression":
    # anything else that fails to compile is a shim defect and fails the gate.
    # The point of the split is that the gate goes red when the shim rots,
    # while still passing on a shim that is merely not finished yet.
    #
    # A shim stands in for SDL. So an entry belongs here only when a shim
    # *cannot* be the answer. Entries that fail for reasons unrelated to SDL
    # are host-toolchain limitations, not shim work; they are listed because
    # the gate compiles on the host, but they must not be read as a work list.
    ShimKnownUncovered = @{
        # Reaches SDL through its own header rather than directly, so the
        # boundary gate's "includes SDL3" test does not see it -- but it is a
        # platform TU in every other sense: an event loop with a scancode
        # keymap table (SDL_SCANCODE_COUNT sizes it) and display queries
        # (SDL_DisplayID, SDL_GetPrimaryDisplay). The port answers keys from
        # its own input layer, so this becomes a port-side event source.
        # THE ONE REAL S20-C IMPLEMENTATION ITEM in this list.
        'src/backend/events/be_events_sdl.c' =
            'port-side event source needed (SDL scancodes, display query)'

        # HOST-ONLY LIMITATION, not S20-C work. Corrected 2026-09-23: this
        # entry used to claim "the target has no sys/stat.h at all, so the
        # port needs its own tchar/mkdir variant". Both halves were wrong,
        # and the second one was a work item that never existed:
        #   * be_filesystem_tchar.h is self-contained off Windows. It defines
        #     _T/TCHAR/_tcslen/_tfopen/_tremove/_tmkdir itself and includes
        #     "tchar.h" only in the Windows arm; no tchar.h exists anywhere in
        #     the tree (find: empty). There was never a header to provide.
        #   * be_filesystem_mkdir.h's non-Windows arm calls _tmkdir(path,0755)
        #     -> mkdir(path,0755). newlib declares the two-argument POSIX form,
        #     so the TARGET compiles both TUs unchanged: arm-none-eabi-gcc
        #     -Wall -Wextra -> rc=0, 0 errors (measured, see
        #     .emv2/discussion/2026-09-22-reflection-keen/filesystem-seam.md).
        #     MinGW's sys/stat.h goes through io.h and declares MSVCRT's
        #     one-argument mkdir, which is why the HOST rejects it.
        # So this is a host-probe limitation and nothing else. Do not turn it
        # into port code; the target-side view lives in
        # Test-FilesystemSeamContract.ps1, which is the honest environment.
        'src/backend/filesystem/be_filesystem_file_ops.c' =
            'host only: MSVCRT mkdir takes one argument; target compiles unchanged'
        'src/backend/filesystem/be_filesystem_gameinst.c' =
            'host only: MSVCRT mkdir takes one argument; target compiles unchanged'
    }

    # How the port takes the filesystem seam. Measured 2026-09-23 on the
    # target toolchain; the reasoning is in filesystem-seam.md.
    #
    # The upstream filesystem directory is already split by platform
    # (be_filesystem_dir_unix.c / be_filesystem_dir_win32.c), so the port adds
    # the third branch and does not compile the other two. No header has to be
    # shadowed, no patch is needed, and upstream stays byte-identical: every
    # remaining property below was established by measurement, not by reading.
    FilesystemSeam = @{
        # Every .c under src/backend/filesystem/ is classified exactly once, so
        # a new upstream file cannot slip in unnoticed.
        RetainedTus = @(
            'be_filesystem_file.c'
            'be_filesystem_file_ops.c'
            'be_filesystem_gameinst.c'
            'be_filesystem_mods.c'
            'be_filesystem_root_paths.c'
        )
        ReplacedTus = @(
            'be_filesystem_app_paths.c'
        )
        ExcludedTus = @{
            'be_filesystem_dir_unix.c' =
                'POSIX dir implementation; the port supplies the third branch'
            'be_filesystem_dir_win32.c' =
                'Win32 dir implementation; the port supplies the third branch'
            'be_filesystem_dirselection.c' =
                'install-directory picker; its BE_Cross_DirSelection_* symbols are called only from backend/launcher/, which is off'
            'be_filesystem_file_lists.c' =
                'save-file listing; its only export is called only from Catacomb (id91_11), and Keen Dreams save slots are fixed names'
        }

        # Symbols the excluded TUs export. None may be referenced by anything
        # the port builds; if that ever becomes false, the exclusion is wrong.
        ExcludedSymbols = @(
            'BE_Cross_DirSelection_'
            'BE_Cross_GetSortedRewritableFilenames'
        )

        # The dir functions upstream declares but implements only per platform.
        # After the exclusions above, be_filesystem_file_ops.c is their only
        # caller, so these four are the whole requirement.
        PortMustProvide = @(
            'BEL_Cross_OpenDir'
            'BEL_Cross_ReadDir'
            'BEL_Cross_RewindDir'
            'BEL_Cross_CloseDir'
        )

        # BEL_Cross_IsDir ships as a static inline whose body is empty for any
        # platform defining neither REFKEEN_PLATFORM_WINDOWS nor
        # REFKEEN_PLATFORM_UNIX -- undefined behaviour, not a compile error, so
        # "it compiles" proves nothing about it. It is dead code for this port
        # through TWO links, and both are checked:
        #   1. the only .c naming it directly is the install-directory picker,
        #      which is excluded;
        #   2. the only caller of the wrapper that inlines it,
        #      BEL_Cross_AddRootPathIfDir, is be_filesystem_app_paths.c, which
        #      the port replaces. Measured: app_paths.c does NOT name
        #      BEL_Cross_IsDir itself -- it calls the wrapper declared in
        #      be_filesystem_root_paths.h. Getting that wrong once already made
        #      this list name the wrong file; hence the two-link form.
        # If either link moves, the empty body stops being dead and the port
        # must stop treating it as harmless.
        IsDirDirectCallers = @(
            'be_filesystem_dirselection.c'
        )
        IsDirWrapperSymbol = 'BEL_Cross_AddRootPathIfDir'
        IsDirWrapperCallers = @(
            'be_filesystem_app_paths.c'
        )
        IsDirPlatformArms = @('REFKEEN_PLATFORM_WINDOWS', 'REFKEEN_PLATFORM_UNIX')

        # BEL_Cross_mkdir returns void, so no call site can depend on whether
        # the directory was created -- upstream's own comment says "just in
        # case". On the target it lands on newlib's mkdir, whose _mkdir_r stub
        # the port does not implement (the shared layer carries no newlib
        # glue: _mkdir_r/_open_r/_stat_r appear only in the SD driver). The
        # port therefore creates its own directories; overriding libc's mkdir
        # would reach outside the port for no gain.
        #
        # Built set only. Upstream has two more sites, in the Android arm of
        # be_filesystem_app_paths.c (lines 177/178), but that TU is replaced,
        # so it is not built and a change there cannot affect the port.
        # Names are bare file names, like RetainedTus above, so the gate's
        # measured value is directly comparable.
        MkidrCallSites = @(
            'be_filesystem_file_ops.c:212'
            'be_filesystem_file_ops.c:220'
            'be_filesystem_gameinst.c:184'
            'be_filesystem_gameinst.c:185'
        )

        # Frozen per-TU diagnostic set, measured on arm-none-eabi-gcc -Wall
        # -Wextra (warning classes only; counts included). Upstream is not
        # -Wall-clean on a platform it was never built for. The port does not
        # paper over this by exempting warnings: it applies -Werror only to its
        # own TUs, exactly as every completed port does (xrick, digger and
        # sdlpop all set -Wall;-Wextra;-Werror on port sources only, and force
        # just -Werror=implicit-function-declaration on the whole target).
        # A change here means upstream moved: re-measure, then decide.
        FrozenDiagnostics = @{
            'be_filesystem_file.c'        = @('-Wunused-function=1')
            'be_filesystem_file_ops.c'    = @('-Wreturn-type=1', '-Wunused-function=1', '-Wunused-parameter=1')
            'be_filesystem_gameinst.c'    = @('-Wunused-function=1', '-Wunused-parameter=1', '-Wunused-variable=2')
            'be_filesystem_mods.c'        = @('-Wunused-function=1')
            'be_filesystem_root_paths.c'  = @('-Wreturn-type=1', '-Wunused-function=1', '-Wunused-parameter=1')
        }
    }

    # How the port replaces src/backend/events/be_events_sdl.c.
    #
    # This is the eighth platform TU: it reaches SDL through its own header,
    # be_events_sdl.h, so the boundary gate's "includes SDL3 directly" test
    # cannot see it -- which is exactly why it was the one platform TU with no
    # record. Measured 2026-09-23; the reasoning is in event-source.md.
    #
    # Dropping it from the build removes a small, fully enumerable set of
    # symbols, and that set is the whole contract the port has to satisfy.
    EventSource = @{
        ReplacedTu = 'src/backend/events/be_events_sdl.c'

        # Non-static definitions at file scope that are NOT inside a
        # REFKEEN_CONFIG_EVENTS_CALLBACK conditional. Every one must be
        # provided by the port; anything else upstream adds here shows up as a
        # gap rather than as a link error three phases later.
        RequiredExports = @(
            'BE_ST_PollEvents'
            'BE_ST_SetAppQuitCallback'
        )

        # Only defined when REFKEEN_CONFIG_EVENTS_CALLBACK is set. That macro
        # belongs to the mobile/background-app ports and stays undefined here
        # (refkeen_config.h), so these do not exist to be provided -- but if the
        # macro is ever defined, they become undefined symbols at link time.
        # The port's replacement therefore carries an #error guard, and the gate
        # asserts the macro is still absent from the port's config header.
        ConditionalExports = @(
            'BEL_ST_CheckForExitFromEventsCallback'
            'BEL_ST_EventsCallback'
            'g_sdlEventsCallbackToMainSem'
            'g_sdlMainToEventsCallbackSem'
        )
        EventsCallbackMacro = 'REFKEEN_CONFIG_EVENTS_CALLBACK'
        ConfigHeader = 'refkeen/host/include/refkeen_config.h'

        # Upstream says at the top of this header that it must be included from
        # exactly one TU, because it defines a non-static table at file scope.
        # The port's replacement becomes that TU; the gate counts includers in
        # the port tree so a second one cannot appear unnoticed.
        KeymapHeader = 'src/backend/events/be_events_sdl_keymap.h'
        KeymapIncluder = 'be_events_port.c'
        KeymapArrayBound = 'SDL_SCANCODE_COUNT'

        # The keymap array is sized by SDL_SCANCODE_COUNT, which upstream gets
        # only transitively, through <SDL3/SDL.h> -> SDL_events.h. Real SDL3
        # provides it; the shim did not, and the port's replacement is the first
        # TU to need it. The shim now includes SDL_scancode.h from SDL.h, and
        # the gate pins that so the umbrella cannot be trimmed back.
        ShimUmbrella = 'refkeen/host/shim/SDL3/SDL.h'
        ShimScancode = 'refkeen/host/shim/SDL3/SDL_scancode.h'

        # Bare names, like the filesystem seam entries, so measured values are
        # directly comparable.
        PortTus = @(
            'be_events_port.c'
            'event_source.c'
        )
        # Only the pure part is host-buildable: the other one includes the
        # engine's own headers and the positional keymap.
        HostCompiledTus = @('event_source.c')
        PortTest = 'test_event_source.c'

        # Measured on arm-none-eabi-gcc -Wall -Wextra. The single warning comes
        # from an unused static inline in upstream be_cross.h, reached through
        # be_st.h -- not from port code. Same policy as the filesystem seam: no
        # -Werror on upstream-headed TUs, and a change here means re-measure.
        FrozenDiagnostics = @{
            'be_events_port.c' = @('-Wunused-function=1')
        }

        # Keyboard repeat state. be_input.c defines all of these and
        # BEL_ST_HandleEmuKeyboardEvent() writes them, so the port's replacement
        # may read and advance them but must not define a second copy: that
        # would be a silent fork of the engine's repeat behaviour.
        SharedRepeatSymbols = @(
            'g_sdlEmuKeyboardLastPressedScanCode'
            'g_sdlEmuKeyboardLastPressedIsSpecial'
            'g_sdlEmuKeyboardLastScanCodePressTime'
            'g_sdlEmuKeyboardLastScanCodePressTimeDelay'
            'g_sdlOnScreenKeyboardLastPressedDirButton'
            'g_sdlOnScreenKeyboardLastDirButtonPressTime'
            'g_sdlOnScreenKeyboardLastDirButtonPressTimeDelay'
        )
        SharedRepeatDefinitionTu = 'src/backend/input/be_input.c'

        # Symbols be_events_sdl.c used that the port's replacement must not
        # name, each with the TU that DEFINES it. Naming one is a link error on
        # the target, which would otherwise not surface until C13.
        #
        # MEASURED 2026-09-23, and the first draft of this list was wrong about
        # THREE of its eight entries. Each mistake is a distinct way to get this
        # wrong again, so they are recorded rather than quietly deleted:
        #
        #   * g_sdlAudioSubsystemUp is DEFINED in be_audio_init.c, a portable TU
        #     the port builds; be_audio_sdl.c only declares it extern. An extern
        #     declaration inside a platform TU is not ownership -- and
        #     be_audio_main_thread.c, also built, reads it too.
        #   * the two window globals are declared on ONE line at be_video.c:51,
        #     "int g_sdlLastReportedWindowWidth, g_sdlLastReportedWindowHeight;"
        #     so a probe looking for "name ... ;" finds neither and concludes
        #     they do not exist. Same comma trap this project keeps hitting.
        #
        # The list is therefore no longer trusted on sight: Check 7 looks up the
        # defining TU of every entry and requires it to lie outside the build
        # set derived from this same file.
        NonBuiltSymbols = @{
            'g_sdlLastReportedWindowWidth'  = 'src/backend/video/be_video.c'
            'g_sdlLastReportedWindowHeight' = 'src/backend/video/be_video.c'
            'g_sdlLastReportedPixelDensity' = 'src/backend/video/be_video.c'
            # The two altcontrol entries that used to live here moved to
            # AltControlStub when C12 supplied a stub for them (2026-09-23).
            # They had to move: this list means "owned by a TU the port does not
            # build, so naming it is a link error", and that stopped being true
            # for g_sdlControllerMappingActualCurr and the
            # BEL_ST_AltControlScheme_* family the moment the port began
            # defining them. Leaving them would have been a wrong record that
            # still passed. The coverage is not lost -- AltControlStub check 3
            # applies the stronger form of the same rule to the whole build set.
        }

        # SDL's own event API. It exists neither in the upstream tree nor in the
        # shim (measured: no SDL_PollEvent, no SDL_Event anywhere under
        # refkeen/host/shim), so the "which TU defines it" question does not
        # apply to these -- naming one is an immediate link error. Check 7
        # verifies both halves so the entry stays load-bearing rather than
        # becoming a comment.
        SdlApiSymbols = @(
            'SDL_PollEvent'
            'SDL_Event'
        )

        # The other direction of the ledger. Upstream globals the replacement
        # DOES name, each with its defining TU, which must be part of the build
        # set. Without this half the check could only ever catch a symbol that
        # must not appear, never one that is silently missing.
        #
        # The four repeat globals the replacement also names are covered by
        # SharedRepeatSymbols / SharedRepeatDefinitionTu in Check 6 and are
        # deliberately not repeated here: one fact, one place.
        NamedBuiltGlobals = @{
            # Assigned at the top of BE_ST_PollEvents() and read by the repeat
            # emulation below it, exactly as upstream does.
            'g_sdlLastPollEventsTime' = 'src/backend/input/be_input.c'

            # Read by the manual-audio-pump arm at the end of BE_ST_PollEvents().
            # be_audio_init.c DEFINES it; be_audio_sdl.c only declares it extern,
            # which is exactly the confusion that made the first draft of
            # NonBuiltSymbols wrong (see there). No header declares it, so the
            # replacement declares it locally -- as upstream's TU does at
            # be_events_sdl.c:50.
            'g_sdlAudioSubsystemUp' = 'src/backend/audio/be_audio_init.c'
        }
    }

    # How the port replaces the altcontrol subsystem.
    #
    # platform-boundary.md 8.2 decided on 2026-09-23 to leave it out (it is
    # ReflectionHLE's modern gamepad/touch mapping feature, which Keen Dreams
    # 1991 does not have), and left the exact stub surface to C12. This block is
    # that surface, measured rather than read.
    #
    # MEASURED 2026-09-23, and the measurement corrected the record twice:
    #
    #   * the S20-B note "altcontroller is 90,624 B" was a HOST number. On the
    #     target (4-byte pointers) the two TUs cost 74,923 B of writable data:
    #     altcontroller.c 56,244 .data + 35 .bss, and
    #     be_input_controller_mappings.c 18,644 .bss. Same class of mistake as
    #     the filesystem seam's MinGW mkdir: a host probe read as a target fact.
    #   * a first pass counted the excluded TUs' exports by reading text and got
    #     54. The objects say 39. Text over-counts twice over: a header
    #     declaration is not a reference, and a prototype inside a .c file is not
    #     a definition (BEL_ST_ReleasePressedKeysInTextInputUI is declared at
    #     be_input_controller_mappings.c:161 and DEFINED in be_video_ui.c:1536).
    #     It also under-counts in one direction: a reference can live in a header
    #     the .c includes, which is how RefKeen_PrepareAltControllerScheme
    #     reaches be_gamedefs_prep.c.
    #
    # The replacement is not free, and that is recorded rather than glossed:
    # nine BE_ST_ControllerMapping objects are still needed (BE_MAX_KEY_ID is
    # 250, so each is 6,204 B), leaving a net RAM saving of 19,083 B and a flash
    # saving of roughly 69 KB. The nine symbols belong on the S20-D PSRAM list.
    AltControlStub = @{
        # Also in PortExcludedTus above; repeated here because this block is the
        # one that measures them. The gate checks the two agree -- one fact, but
        # it is read by two questions.
        ExcludedTus = @(
            'src/kdreams/altcontroller.c'
            'src/backend/input/be_input_controller_mappings.c'
        )

        PortTus = @('altcontrol_stub.c')
        PortTest = 'test_altcontrol_stub.c'

        # Every symbol the two excluded TUs export that a built TU asks for.
        # Measured: compile all 62 built TUs of the Keen Dreams target, take the
        # union of their undefined symbols, and intersect with the excluded TUs'
        # 39 definitions. The port's stub defines exactly these and nothing else.
        RequiredSymbols = @(
            'BEL_ST_AltControlScheme_ClearBinaryStates'
            'BEL_ST_AltControlScheme_HandleEntry'
            'BE_ST_AltControlScheme_Pop'
            'BE_ST_AltControlScheme_PrepareControllerMapping'
            'BE_ST_AltControlScheme_Push'
            'BE_ST_AltControlScheme_Reset'
            'BE_ST_AltControlScheme_UpdateVirtualMouseCursor'
            'FinalizeControlPanelMappingsByMousePresence'
            'PrepareGamePlayControllerMapping'
            'RefKeen_PrepareAltControllerScheme'
            'UpdateGameplayMappingsByMousePresence'
            'g_beStControllerMappingDebugKeys'
            'g_beStControllerMappingTextInput'
            'g_binding_value_button'
            'g_binding_value_down'
            'g_binding_value_left'
            'g_binding_value_right'
            'g_binding_value_stats'
            'g_binding_value_up'
            'g_ingame_altcontrol_mapping_demoloop'
            'g_ingame_altcontrol_mapping_inackback'
            'g_ingame_altcontrol_mapping_menu'
            'g_ingame_altcontrol_mapping_menu_help'
            'g_ingame_altcontrol_mapping_notenoughmemorytostart'
            'g_ingame_altcontrol_mapping_simpledialog'
            'g_keybind_used_button'
            'g_keybind_used_down'
            'g_keybind_used_left'
            'g_keybind_used_right'
            'g_keybind_used_stats'
            'g_keybind_used_up'
            'g_sdlControllerMappingActualCurr'
        )

        # The rest of the 39. Each is defined only in a replaced TU and called
        # only from a replaced TU, so dropping it is not a gap -- but saying why
        # is what keeps it from being one later.
        DroppedSymbols = @{
            'BEL_ST_AltControlScheme_CleanUp' =
                'called only from BEL_ST_AltControlScheme_HandleAnyEntry/Pop/Reset, all in the replaced TU'
            'BEL_ST_AltControlScheme_ConditionallyShowOnScreenControls' =
                'called only from the replaced TU; it is the on-screen-control show/hide step'
            'BEL_ST_AltControlScheme_HandleAnyEntry' =
                'called only by BEL_ST_AltControlScheme_HandleEntry, which the port stubs'
            'BEL_ST_AltControlScheme_HandleDebugKeysEvent' =
                'on-screen debug-keys UI; called only from be_events_sdl.c, replaced in C9'
            'BEL_ST_AltControlScheme_HandleTextInputEvent' =
                'on-screen keyboard; called only from be_events_sdl.c, replaced in C9'
            'g_ingame_altcontrol_mapping_funckeys' =
                'referenced only from RefKeen_PrepareAltControllerScheme, in the replaced TU'
            'g_ingame_altcontrol_mapping_gameplay' =
                'referenced only from the replaced TU'
        }

        # 39 = 32 + 7. The gate re-measures this from the objects, so a change
        # upstream cannot pass unnoticed.
        ExcludedDefinitionCount = 39

        # Where the Keen Dreams target's TU list comes from, and what it is worth.
        # Upstream builds a Keen Dreams binary as kdreams_src + be_src, evaluated
        # with these CMake variables. The gate re-reads both files and re-derives
        # the list, rather than trusting a frozen copy of 76 paths.
        SourceLists = @(
            'src/CMakeLists.txt'
            'src/kdreams/CMakeLists.txt'
        )
        CmakeVars = @{ 'WIN32' = $false; 'LINUX' = $false; 'REFKEEN_ENABLE_LAUNCHER' = $false }

        # Paths those files list inside a conditional arm this target does not
        # take. Named, not counted, so that upstream adding a tenth one is a
        # failure rather than a silently larger build set.
        ConditionalArms = @(
            'src/backend/filesystem/be_filesystem_dir_win32.c'
            'src/backend/misc/be_misc_winreg.c'
            'src/backend/misc/be_misc_lpt_linux.c'
            'src/backend/misc/be_misc_lpt_win32.c'
            'src/backend/launcher/be_launcher.c'
            'src/backend/launcher/be_launcher_cfg.c'
            'src/backend/launcher/be_launcher_cfg_sdl.c'
            'src/backend/launcher/be_st_sdl_launcher.c'
        )

        # In the target, removed by the port's own decisions. This is the other
        # half of the "not built" set: derived at gate time, compared against the
        # facts the other gates use, so the two cannot disagree.
        PortRemovedTus = @{
            'src/actual_main.c' =
                'port responsibility (AllowedOutsideBackend); the port supplies main()'
            'src/backend/audio/be_audio_sdl.c' = 'platform TU'
            'src/backend/cfg/be_cfg_sdl.c' = 'platform TU'
            'src/backend/filesystem/be_filesystem_app_paths.c' = 'platform TU / replaced (C10)'
            'src/backend/input/be_input_sdl.c' = 'platform TU'
            'src/backend/startup/be_startup_sdl.c' = 'platform TU'
            'src/backend/video/be_video.c' = 'platform TU'
            'src/backend/video/be_video_sdl.c' = 'platform TU'
            'src/backend/events/be_events_sdl.c' = 'replaced by the port event source (C9)'
            'src/backend/filesystem/be_filesystem_dir_unix.c' = 'port supplies the dir branch (C10)'
            'src/backend/filesystem/be_filesystem_dirselection.c' = 'launcher-only install-directory picker'
            'src/backend/filesystem/be_filesystem_file_lists.c' = 'Catacomb-only save listing'
            'src/kdreams/altcontroller.c' = 'altcontrol decision (platform-boundary.md 8.2)'
            'src/backend/input/be_input_controller_mappings.c' = 'altcontrol decision (same)'
        }

        TargetTuCount = 76        # 50 be_src (Keen Dreams arm) + 26 kdreams_src
        BuildTuCount = 62         # target minus PortRemovedTus

        # Measured on the target toolchain, arm-none-eabi-gcc -mcpu=cortex-m33
        # -mthumb -O0. The .bss is nine BE_ST_ControllerMapping objects at 6,204 B
        # each plus alignment; the pointer itself is the 4 B of .data. If the
        # mapping type grows, this changes and the gate says so.
        StubTextBytes = 236
        StubDataBytes = 4
        StubBssBytes = 55836
        StubExportedCount = 32

        # The other side of the same ledger, measured the same way: the replaced
        # TUs' writable data (.data + .bss, which is what they cost in RAM) and
        # their footprint in flash (.text + .rodata + the .data initial image).
        # Recorded so the gate compares two measurements instead of only
        # reporting one -- a ledger that is only printed cannot be wrong.
        RemovedRamBytes = 74927
        RemovedFlashBytes = 68927

        # Measured: one unused static in upstream be_cross.h, reached through
        # be_st.h -> be_misc_lpt.h. Not port code, and the same policy as the
        # other seams -- no -Werror on a TU that includes upstream headers.
        FrozenDiagnostics = @{
            'altcontrol_stub.c' = @('-Wunused-function=1')
        }
        # Twice as many on the host, and the reason is the invocation rather than
        # the code: the host test's build compiles TWO TUs (the stub and the
        # test), and each one includes be_st.h, so upstream's unused static is
        # reported once per TU. Recorded as 2 only after the gate caught the
        # mismatch -- the measurement was right and the first draft of this line
        # was wrong, which is the whole point of freezing it next to the command
        # that produces it.
        HostDiagnostics = @('-Wunused-function=2')

        # The premise the whole stub rests on, asserted at compile time by
        # refkeen/tests/test_altcontrol_stub.c rather than only written down.
        ZeroIsEmptyMapping = 'BE_ST_CTRL_MAP_NONE == 0'
    }

    # -----------------------------------------------------------------------
    # C11 -- the port's use of the shared public frontend
    # (common/frontend/{frontend_shell,native_game_settings,settings_host}.c).
    #
    # Unlike the other blocks, most of the numbers here are not about an
    # upstream surface the port has to replace: they are the port's own
    # decisions, and the gate's job is to notice when one of them drifts --
    # including when the shared layer moves underneath (a lifecycle version bump
    # changes LifecycleVersion, a new callback changes the section sizes, and a
    # new row on the Settings page changes the mapping values).
    # -----------------------------------------------------------------------
    Frontend = @{
        PortTu = 'frontend.c'
        PortTest = 'test_frontend_adapter.c'
        HookHeader = 'frontend_hook.h'

        # The C9 event TU, which is what actually runs the hook. See Frontend's
        # banner in frontend.c for why the hook has to live there.
        CallerTu = 'be_events_port.c'
        PumpSymbol = 'retropicoes_refkeen_frontend_pump'
        ResetSymbol = 'retropicoes_refkeen_event_source_reset'

        # be_events_port.c runs the hook; frontend.h includes it so there is one
        # definition of the bits; and the host test includes it to assert the bit
        # values against the shared enum's numbering. Nothing else, and in
        # particular nothing from the engine or the shared frontend -- the event
        # TU's dependency surface is a measured contract.
        HookIncluders = @(
            'be_events_port.c'
            'frontend.h'
            'test_frontend_adapter.c'
        )

        # Measured: arm-none-eabi-nm --defined-only -g on the target object at
        # -mcpu=cortex-m33 -mthumb -O0. The six lifecycle callbacks are static
        # and must stay static; the gate fails if one leaks into the ABI.
        ExportedSymbols = @(
            'retropicoes_refkeen_frontend_boot'
            'retropicoes_refkeen_frontend_pump'
            'retropicoes_refkeen_frontend_finished'
            'retropicoes_refkeen_frontend_shutdown'
            'retropicoes_refkeen_frontend_map_action'
            'retropicoes_refkeen_frontend_is_paused'
            'retropicoes_refkeen_frontend_quit_requested'
            'retropicoes_refkeen_frontend_loader_requested'
        )
        ExportedCount = 8

        # Measured the same way, and explained rather than merely recorded:
        #
        #   .text   628  the eight functions
        #   .rodata  64  36 for the lifecycle literal + 28 for the settings path
        #   .bss     64  44 for the shell struct + 20 for five int flags
        #   .data     0  nothing is initialised at file scope
        #
        # The 44 is the interesting one. retropicoes_frontend_shell_t is 36 bytes
        # of lifecycle plus four enum-ish members, and on this toolchain the four
        # cost 8 bytes, not 16: arm-none-eabi-gcc defaults to -fshort-enums, so
        # the shared struct is 44 bytes here against 56 with 4-byte enums. A
        # decision made about this struct's size on the host would be wrong here;
        # the same class of error as the altcontrol mapping table, which is 9,808
        # on the host and 6,204 on the target.
        TextBytes = 628
        DataBytes = 0
        BssBytes = 64
        RodataBytes = 64

        # The undefined set of the same object: what the linker will have to
        # resolve, i.e. the shared surface this port actually consumes. Five
        # frontend_shell calls, three native_game_settings calls, and the three
        # runtime/port hooks. No libc call at all, and no rom_browser.
        SharedCalls = @(
            'retropicoes_frontend_shell_game_finished'
            'retropicoes_frontend_shell_init'
            'retropicoes_frontend_shell_launch'
            'retropicoes_frontend_shell_open_settings'
            'retropicoes_frontend_shell_settings_action'
            'retropicoes_input_menu_pressed'
            'retropicoes_native_game_scanlines_enabled'
            'retropicoes_native_game_settings_init'
            'retropicoes_native_game_settings_run'
            'retropicoes_refkeen_event_source_quit'
            'retropicoes_video_set_scanlines'
        )

        # Measured: the adapter compiles on the target with -Wall -Wextra and
        # emits nothing. Zero is a claim the self-test has to break, so it does:
        # one injected cast-away-const makes check 2 fire.
        FrozenDiagnostics = @()

        # The host build compiles the test, the adapter and the REAL shared
        # frontend_shell.c, under -Werror. Also empty, also injected in the
        # self-test rather than assumed.
        HostDiagnostics = @()

        # The host test's own statement of the same facts, so the gate compares
        # two derivations instead of re-reading one.
        HostAssertions = 76
        LifecycleVersion = 1
        # sizeof(retropicoes_frontend_lifecycle_t) on the pinned host compiler.
        # 72 = 4 + 4 padding + 8 (size_t) + 8 (pointer) + 6 x 8. A different
        # number means the shared lifecycle struct changed and the port needs
        # re-reading -- which is the whole reason this is frozen here.
        LifecycleSizeHost = 72
        BootStageOk = 0
        SettingsPath = '/SAVES/refkeen/settings.dat'

        # The three-way answer, as the shell enumerates it. Quit -> ROM_BROWSER
        # is the pair worth re-reading: a native game's ROM list is empty.
        MapResume = 1
        MapQuit = 3
        MapLoader = 4

        # The bits the caller reads. IDLE is 0 on purpose: a caller that ignores
        # the return value must not accidentally act on anything.
        BitsIdle = 0
        BitsSettings = 1
        BitsQuit = 2
        BitsLoader = 4

        # The primitive the quit path names. be_startup_sdl.c is one of the TUs
        # the port replaces, so naming this symbol is only safe because the port
        # supplies it -- which is C13's obligation, recorded here so it cannot be
        # forgotten silently. The gate measures both halves: the name is present
        # in the port, and the only defining TU in upstream's src is the removed
        # one. C13 adds the port definition itself and its focused host test;
        # the measured naming set therefore contains the caller, supplier and
        # test, rather than only the pre-C13 caller.
        QuitPrimitive = 'BE_ST_QuickExit'
        QuitPrimitiveTu = 'src/backend/startup/be_startup_sdl.c'
        QuitPrimitiveNamingTus = @(
            'be_events_port.c'
            'be_startup_port.c'
            'test_startup_port.c'
        )
        PortSuppliedSymbols = @('BE_ST_QuickExit')

        # be_startup_sdl.c is a full SDL platform TU, so the port's shim cannot
        # compile it and is not meant to. The host baseline builds it against
        # real SDL3, and that is where the definition is observed. Pinned to the
        # S20-B SDL3 version on purpose: the gate SKIPs loudly if the mirror is
        # gone rather than quietly checking less.
        Sdl3HostInclude = '_deps\SDL3-3.4.16\x86_64-w64-mingw32\include'
        QuitTuDefines = @(
            '-DREFKEEN_VER_KDREAMS=1'
            '-DREFKEEN_HAS_VER_KDREAMS=1'
        )

        # The RetroPicoES runtime calls the port uses. There is no shared header
        # for them -- every core declares them in its own platform/retropicoes
        # copy -- so the port's frontend.h is cross-checked against digger's and
        # xrick's rather than allowed to invent an interface.
        # A data file does accept a line break after '=', but each of these fits
        # on one line, and a one-line entry cannot be broken by an edit that
        # swallows the newline between two keys.
        RuntimeDecls = @{
            'retropicoes_input_menu_pressed' = 'int retropicoes_input_menu_pressed(void);'
            'retropicoes_video_set_scanlines' = 'void retropicoes_video_set_scanlines(int enabled);'
            'retropicoes_launched_from_bootloader' = 'int retropicoes_launched_from_bootloader(void);'
            'retropicoes_return_to_bootloader' = 'void retropicoes_return_to_bootloader(void);'
        }
        SiblingRuntimeHeaders = @(
            'digger\platform\retropicoes\input_backend.h'
            'digger\platform\retropicoes\video_backend.h'
            'digger\platform\retropicoes\runtime.h'
            'xrick\platform\retropicoes\input_backend.h'
            'xrick\platform\retropicoes\video_backend.h'
            'xrick\platform\retropicoes\runtime.h'
        )

        # common/frontend/scripts/Test-FrontendAdoption.ps1 skips its S20 arm
        # until refkeen/pico/CMakeLists.txt exists, so these are the same tokens
        # applied early -- while the port can still be fixed cheaply.
        CmakeTokens = @('native_game_settings.c', 'frontend_shell.c', 'settings_host.c')
    }
}
