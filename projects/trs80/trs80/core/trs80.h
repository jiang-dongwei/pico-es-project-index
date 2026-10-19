/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PICOES_TRS80_H
#define PICOES_TRS80_H
#include <stddef.h>
#include <stdint.h>
typedef enum { TRS80_MODEL_I = 1, TRS80_MODEL_III = 3, TRS80_MODEL_4 = 4 } trs80_model_t;
typedef enum { TRS80_OK, TRS80_INVALID, TRS80_TRUNCATED, TRS80_UNSUPPORTED,
               TRS80_PROTECTED, TRS80_NO_ENTRY, TRS80_CPU_FAULT } trs80_result_t;
typedef struct {
    uint16_t pc, sp, af, bc, de, hl, ix, iy;
    uint64_t cycles;
    uint32_t clock_hz;
    unsigned columns, rows, memory_map;
    int fault;
    unsigned video_offset, inverse, alternate;
} trs80_status_t;
#define TRS80_MAX_BIOS_BYTES 0x3800u
typedef enum { TRS80_DISK_JV1 = 1, TRS80_DISK_JV3 = 3 } trs80_disk_format_t;
/* Mounted buffers remain owned by the caller and must outlive the mount. */
trs80_result_t trs80_mount_disk(unsigned drive, const uint8_t *data, size_t size,
                               trs80_disk_format_t format);
void trs80_unmount_disks(void);
typedef void (*trs80_audio_sink_t)(int16_t sample, void *context);
/* Sink runs on the emulation thread at 22050 Hz. NULL disables sampling. */
void trs80_set_audio_sink(trs80_audio_sink_t sink, void *context);
trs80_result_t trs80_bind_expansion_ram(uint8_t *data, size_t size);
#define TRS80_MAX_CMD_BYTES (1024u * 1024u)
#define TRS80_MATRIX_ENTER 48u
#define TRS80_MATRIX_CLEAR 49u
#define TRS80_MATRIX_BREAK 50u
#define TRS80_MATRIX_UP 51u
#define TRS80_MATRIX_DOWN 52u
#define TRS80_MATRIX_LEFT 53u
#define TRS80_MATRIX_RIGHT 54u
#define TRS80_MATRIX_SPACE 55u
#define TRS80_MATRIX_SHIFT 56u
#define TRS80_MATRIX_CTRL 58u
/* xtrs owns one CPU instance; calls belong to the emulation thread. */
trs80_result_t trs80_init(trs80_model_t model);
void trs80_reset(void);
trs80_result_t trs80_load_bios(const uint8_t *data, size_t size);
trs80_result_t trs80_load_cmd(const uint8_t *data, size_t size);
trs80_result_t trs80_run_cycles(uint32_t cycles);
uint8_t trs80_read(uint16_t address);
void trs80_write(uint16_t address, uint8_t value);
uint8_t trs80_in(uint8_t port);
void trs80_out(uint8_t port, uint8_t value);
int trs80_key(unsigned matrix_key, int down);
void trs80_release_keys(void);
const uint8_t *trs80_video(size_t *size);
trs80_status_t trs80_status(void);
#endif
