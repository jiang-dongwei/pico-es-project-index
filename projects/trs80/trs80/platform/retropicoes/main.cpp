/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cstdio>
#include <cstring>
#include <cctype>
#include "hardware/uart.h"
#include "settings_host.h"
extern "C" {
#include "compat.h"
#include "trs80.h"
#include "virtual_keyboard.h"
#include "video_adapter.h"
#include "media_model.h"
#include "frontend_shell.h"
#include "input_backend.h"
#include "input_mapping.h"
#include "video_backend.h"
#include "runtime.h"
#include "audio_backend.h"
}
namespace {
struct Entry { char name[96]; bool directory; } entries[96];
size_t entry_count;
size_t remembered_selection, remembered_first;
struct History { size_t selected, first; } history[16];
unsigned depth;
char directory[320] = "/roms/TRS80", error_text[120];
uint8_t *media;
size_t media_size;
trs80_model_t model = TRS80_MODEL_I;
trs80_model_t active_model = TRS80_MODEL_I;
int scanlines, demo, cmd_loaded;
int audio_enabled = 1;
int16_t audio_ring[2048];
uint32_t audio_read, audio_write, audio_dropped, audio_missing;
void audio_produce(int16_t value, void *)
{
    uint32_t w = audio_write, r = __atomic_load_n(&audio_read,__ATOMIC_ACQUIRE);
    if (w-r >= 2048u) { ++audio_dropped; return; }
    audio_ring[w & 2047u] = value;
    __atomic_store_n(&audio_write,w+1u,__ATOMIC_RELEASE);
}
void audio_consume(int16_t *samples, size_t count, void *)
{
    uint32_t r = audio_read, w = __atomic_load_n(&audio_write,__ATOMIC_ACQUIRE);
    unsigned missing=0;
    for (size_t i=0;i<count;++i) {
        if (r != w) samples[i] = audio_ring[(r++) & 2047u];
        else { samples[i]=0; ++missing; }
    }
    __atomic_fetch_add(&audio_missing,missing,__ATOMIC_RELAXED);
    __atomic_store_n(&audio_read,r,__ATOMIC_RELEASE);
}
void audio_pause()
{
    compat_audio_stop(); trs80_set_audio_sink(nullptr,nullptr); audio_read=audio_write=0;
}
void audio_resume()
{
    audio_pause();
    if (!audio_enabled || demo) return;
    trs80_set_audio_sink(audio_produce,nullptr);
    if (compat_audio_start(22050,1,audio_consume,nullptr) != 0) trs80_set_audio_sink(nullptr,nullptr);
}
struct Perf {
    uint64_t start, heartbeat, cpu, draw, input, present, wait, total, peak;
    unsigned frames, over, uart_drop, dropped_base, missing_base;
    retropicoes_video_stats_t video;
    retropicoes_audio_stats_t audio;
    char pending[640]; size_t cursor, length;
    void clear() { cpu=draw=input=present=wait=total=peak=0; frames=over=0; }
    void begin() {
        start=compat_ticks_us(); heartbeat=start; clear(); length=cursor=0; uart_drop=0;
        dropped_base=audio_dropped; missing_base=__atomic_load_n(&audio_missing,__ATOMIC_RELAXED);
        retropicoes_video_get_stats(&video); retropicoes_audio_get_stats(&audio);
    }
    void frame(uint64_t c, uint64_t d, uint64_t i, uint64_t p, uint64_t w, uint64_t t) {
        cpu+=c; draw+=d; input+=i; present+=p; wait+=w; total+=t;
        if (t>peak) peak=t;
        if (t>16667u) ++over;
        ++frames; uint64_t now=compat_ticks_us();
        if (now-start >= 2000000u) {
            retropicoes_video_stats_t vs{}; retropicoes_video_get_stats(&vs);
            retropicoes_audio_stats_t as{}; retropicoes_audio_get_stats(&as);
            unsigned missing=__atomic_load_n(&audio_missing,__ATOMIC_RELAXED);
            if (cursor==length) {
                uint64_t elapsed=now-start;
                unsigned fps=static_cast<unsigned>(frames*100000000ull/elapsed);
                int n=std::snprintf(pending,sizeof(pending),
                    "PERF TRS80 window_us=%llu fps=%u.%02u target=60 display=%lu refresh=%lu avg_us input=%llu step=%llu draw=%llu present=%llu wait=%llu audio=async total=%llu peak=%llu budget=16667 over=%u/%u load=%llu%%\r\n"
                    "PERF IO TRS80 window_us=%llu audio_under=%lu audio_missing=%u audio_drop=%u video_drop=%lu resync=%lu uart_drop=%u\r\n",
                    static_cast<unsigned long long>(elapsed),fps/100u,fps%100u,
                    static_cast<unsigned long>((vs.displayed_frames-video.displayed_frames)*1000000ull/elapsed),
                    static_cast<unsigned long>((vs.hstx_frames-video.hstx_frames)*1000000ull/elapsed),
                    static_cast<unsigned long long>(input/frames),static_cast<unsigned long long>(cpu/frames),
                    static_cast<unsigned long long>(draw/frames),static_cast<unsigned long long>(present/frames),
                    static_cast<unsigned long long>(wait/frames),static_cast<unsigned long long>(total/frames),static_cast<unsigned long long>(peak),over,frames,
                    static_cast<unsigned long long>(cpu*100u/frames/16667u),
                    static_cast<unsigned long long>(elapsed),static_cast<unsigned long>(as.underruns-audio.underruns),
                    missing-missing_base,static_cast<unsigned>(audio_dropped-dropped_base),
                    static_cast<unsigned long>(vs.dropped_frames-video.dropped_frames),static_cast<unsigned long>(vs.resync_count-video.resync_count),uart_drop);
                length = n > 0 && static_cast<size_t>(n)<sizeof(pending) ? static_cast<size_t>(n) : 0; cursor=0;
                if (!length) ++uart_drop;
            } else ++uart_drop;
            start=now; clear(); video=vs; audio=as; missing_base=missing; dropped_base=audio_dropped;
        }
        if (now-heartbeat >= 10000000u && cursor==length) {
            int n=std::snprintf(pending,sizeof(pending),"TRS80 HEARTBEAT pc=%04x cols=%u rows=%u\r\n",trs80_status().pc,trs80_status().columns,trs80_status().rows);
            length=n>0 ? static_cast<size_t>(n) : 0; cursor=0; heartbeat=now;
        }
        unsigned budget=64;
        while (cursor<length && budget-- && uart_is_writable(uart0)) uart_putc_raw(uart0,pending[cursor++]);
    }
} perf;
retropicoes_frontend_shell_t shell;
retropicoes_rom_browser_t browser;
retropicoes_menu_owner_t owner{RETROPICOES_MENU_NO_CONTROLLER};
retropicoes_controller_profile_t profile = RETROPICOES_CONTROLLER_UNKNOWN;
retropicoes_settings_home_gate_t home_gate;
retropicoes_frontend_settings_action_t requested;
unsigned saved_model, saved_scanlines, saved_audio;
retropicoes_settings_option_id_t options[8];
size_t option_count;
retropicoes_ui_actions_t map(uint32_t mask)
{
    auto a = retropicoes_menu_face_actions_from_compat(profile,
        mask & (1u << COMPAT_KEY_ACTION), mask & (1u << COMPAT_KEY_JUMP));
    const unsigned keys[] = {COMPAT_KEY_UP,COMPAT_KEY_DOWN,COMPAT_KEY_LEFT,COMPAT_KEY_RIGHT};
    const unsigned ui[] = {RETROPICOES_UI_UP,RETROPICOES_UI_DOWN,RETROPICOES_UI_LEFT,RETROPICOES_UI_RIGHT};
    for (unsigned i = 0; i < 4; ++i) if (mask & (1u << keys[i])) a |= ui[i];
    if (mask & (1u << COMPAT_KEY_BACK)) a |= RETROPICOES_UI_MENU;
    if (mask & (1u << COMPAT_KEY_START)) a |= RETROPICOES_UI_AUX;
    return a;
}
uint32_t mask(bool pressed)
{
    uint32_t v = 0;
    for (unsigned i = 0; i < COMPAT_KEY_COUNT; ++i)
        if (pressed ? compat_key_pressed(static_cast<compat_key_t>(i)) : compat_key_down(static_cast<compat_key_t>(i))) v |= 1u << i;
    return v;
}
void reset_owner(void *) { retropicoes_menu_owner_reset(&owner); profile = RETROPICOES_CONTROLLER_UNKNOWN; }
void update_owner(void *)
{
    retropicoes_input_stats_t s{}; retropicoes_input_get_stats(&s);
    uint32_t actions[4];
    for (unsigned i = 0; i < 4; ++i) actions[i] = s.source_masks[i];
    if (owner.active_controller < 2 && !s.usb_name[owner.active_controller]) retropicoes_menu_owner_disconnected(&owner,owner.active_controller);
    size_t n = retropicoes_menu_owner_update(&owner,actions,4);
    if (n < 2) profile = retropicoes_menu_controller_profile_from_name(s.usb_name[n]);
    else if (n < 4) profile = s.nes_pad_type[n-2] == RETROPICOES_NESPAD_SNES ? RETROPICOES_CONTROLLER_SNES : RETROPICOES_CONTROLLER_NES;
}
auto labels() { return retropicoes_menu_controller_labels(profile); }
auto surface()
{
    auto *f = compat_framebuffer();
    return retropicoes_menu_surface_t{f->pixels,f->stride,f->width,f->height};
}
void message(const char *title, const char *body)
{
    auto s = surface(); auto r = retropicoes_menu_nintendo_renderer();
    const char *rows[] = {body,"B: BACK   MENU: SETTINGS"};
    retropicoes_menu_view_t v{title,rows,2,0,0,2,labels(),nullptr};
    retropicoes_menu_render(&r,&s,&v); compat_present();
}
bool extension(const char *name, const char *ext)
{
    auto *dot = std::strrchr(name,'.'); if (!dot) return false;
    while (*dot && *ext) if (std::toupper(static_cast<unsigned char>(*dot++)) != *ext++) return false;
    return !*dot && !*ext;
}
void scan()
{
    entry_count = 0;
    compat_dir_t *d = compat_dir_open(directory); if (!d) return;
    const char *name;
    while ((name = compat_dir_read(d)) && entry_count < 96) {
        if (*name == '.' || std::strlen(name) >= sizeof(entries[0].name) || std::strpbrk(name,"/\\:")) continue;
        char path[420]; std::snprintf(path,sizeof(path),"%s/%s",directory,name);
        bool dir = compat_path_type(path) == COMPAT_PATH_DIRECTORY;
        if (!dir && !extension(name,".CMD") && !extension(name,".JV1") && !extension(name,".JV3")) continue;
        auto &e = entries[entry_count++]; std::strcpy(e.name,name); e.directory = dir;
    }
    compat_dir_close(d);
    for (size_t i = 1; i < entry_count; ++i) for (size_t j = i; j && std::strcmp(entries[j-1].name,entries[j].name)>0; --j) {
        Entry e = entries[j]; entries[j] = entries[j-1]; entries[j-1] = e;
    }
}
size_t count(void *) { return entry_count; }
int item(void *, size_t n, retropicoes_rom_item_view_t *v)
{
    if (n >= entry_count) return 0;
    const char *label = entries[n].name;
    if (entries[n].directory && !std::strcmp(directory,"/roms/TRS80")) {
        if (!std::strcmp(label,"Model_I")) label="Model I";
        else if (!std::strcmp(label,"Model_III")) label="Model III";
        else if (!std::strcmp(label,"Model_4")) label="Model 4";
    }
    *v = {label,entries[n].directory ? RETROPICOES_ROM_ITEM_DIRECTORY : RETROPICOES_ROM_ITEM_ROM}; return 1;
}
const char *path(void *) { return directory; }
bool read_file(const char *name, uint8_t **out, size_t *size, size_t limit)
{
    compat_file_t *f = compat_file_open(name,"rb"); if (!f) return false;
    long n = compat_file_size(f);
    if (n <= 0 || static_cast<size_t>(n) > limit) { compat_file_close(f); return false; }
    auto *p = static_cast<uint8_t *>(compat_large_malloc(static_cast<size_t>(n)));
    bool ok = p && compat_file_read(p,1,static_cast<size_t>(n),f) == static_cast<size_t>(n);
    compat_file_close(f); if (!ok) { compat_free(p); return false; }
    *out = p; *size = static_cast<size_t>(n); return true;
}
retropicoes_menu_result_t pause(void *) { audio_pause(); trs80_release_keys(); return RETROPICOES_MENU_RESULT_OK; }
retropicoes_menu_result_t resume(void *) { audio_resume(); perf.begin(); trs80_release_keys(); return RETROPICOES_MENU_RESULT_OK; }
retropicoes_menu_result_t stop(void *)
{
    audio_pause(); trs80_unmount_disks(); compat_free(media); media = nullptr; media_size = 0;
    trs80_release_keys(); return RETROPICOES_MENU_RESULT_OK;
}
trs80_result_t start_cmd()
{
    trs80_result_t result = trs80_run_cycles(trs80_status().clock_hz * 2u);
    const unsigned boot_keys[] = {12u,TRS80_MATRIX_ENTER,TRS80_MATRIX_ENTER};
    for (unsigned i = active_model == TRS80_MODEL_I ? 1u : 0u; i < 3u && result == TRS80_OK; ++i) {
        trs80_key(boot_keys[i],1); result = trs80_run_cycles(trs80_status().clock_hz/10u);
        trs80_release_keys();
        if (result == TRS80_OK) result = trs80_run_cycles(trs80_status().clock_hz/2u);
    }
    return result == TRS80_OK ? trs80_load_cmd(media,media_size) : result;
}
retropicoes_menu_result_t launch(void *, size_t index, retropicoes_rom_item_kind_t kind)
{
    stop(nullptr); demo = kind == RETROPICOES_ROM_ITEM_DEMO;
    if (demo) return RETROPICOES_MENU_RESULT_OK;
    if (index >= entry_count) return RETROPICOES_MENU_RESULT_INVALID;
    active_model = trs80_media_model(directory,model);
    trs80_init(active_model);
    char bios_path[80]; std::snprintf(bios_path,sizeof(bios_path),"/data/trs80/bios/model%u.rom",static_cast<unsigned>(active_model));
    uint8_t *bios = nullptr; size_t size = 0;
    if (!read_file(bios_path,&bios,&size,TRS80_MAX_BIOS_BYTES)) {
        std::snprintf(error_text,sizeof(error_text),"MISSING BIOS: MODEL %u",static_cast<unsigned>(active_model)); return RETROPICOES_MENU_RESULT_FAILED;
    }
    auto result = trs80_load_bios(bios,size); compat_free(bios);
    char name[420]; std::snprintf(name,sizeof(name),"%s/%s",directory,entries[index].name);
    if (result != TRS80_OK || !read_file(name,&media,&media_size,TRS80_MAX_CMD_BYTES)) {
        std::strcpy(error_text,"BIOS OR MEDIA READ FAILED"); return RETROPICOES_MENU_RESULT_FAILED;
    }
    cmd_loaded = extension(name,".CMD");
    if (cmd_loaded) result = start_cmd();
    else result = trs80_mount_disk(0,media,media_size,extension(name,".JV1") ? TRS80_DISK_JV1 : TRS80_DISK_JV3);
    if (result != TRS80_OK) { std::snprintf(error_text,sizeof(error_text),"UNSUPPORTED MEDIA OR LOAD ERROR %u",static_cast<unsigned>(result)); stop(nullptr); return RETROPICOES_MENU_RESULT_FAILED; }
    std::printf("TRS80 LOAD model=%u media=%s format=%s\n",static_cast<unsigned>(active_model),name,cmd_loaded ? "CMD" : (extension(name,".JV1") ? "JV1" : "JV3"));
    audio_resume(); perf.begin();
    return RETROPICOES_MENU_RESULT_OK;
}
retropicoes_menu_result_t reset(void *)
{
    if (demo) return RETROPICOES_MENU_RESULT_OK;
    audio_pause(); trs80_reset();
    if (cmd_loaded && start_cmd() != TRS80_OK) return RETROPICOES_MENU_RESULT_FAILED;
    audio_resume(); return RETROPICOES_MENU_RESULT_OK;
}
retropicoes_menu_result_t loader(void *) { stop(nullptr); retropicoes_return_to_bootloader(); return RETROPICOES_MENU_RESULT_OK; }
void renderer(void *, retropicoes_menu_renderer_t *r) { *r = retropicoes_menu_nintendo_renderer(); }
retropicoes_controller_profile_t controller(void *) { return profile; }
retropicoes_ui_actions_t read_actions(void *, retropicoes_menu_repeat_t *repeat)
{
    auto held = map(mask(false)), pressed = map(mask(true));
    if (retropicoes_input_menu_pressed()) pressed |= RETROPICOES_UI_MENU;
    return (pressed & ~(RETROPICOES_UI_UP | RETROPICOES_UI_DOWN)) |
        retropicoes_menu_repeat_update(repeat,held,pressed,compat_ticks_ms());
}
void settings_render(void *, const retropicoes_settings_menu_state_t *state)
{
    retropicoes_settings_item_view_t views[8]{};
    for (size_t i = 0; i < option_count; ++i) {
        auto id = options[i]; views[i].label = retropicoes_settings_common_label(id);
        views[i].help = retropicoes_settings_common_help(id); views[i].value = "";
        if (id == RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE) {
            views[i] = {"Machine Model",model == TRS80_MODEL_I ? "I" : (model == TRS80_MODEL_III ? "III" : "4"),"FALLBACK; MODEL FOLDERS SELECT AUTOMATICALLY"};
        } else if (id == RETROPICOES_SETTINGS_OPTION_SCANLINES) views[i].value = scanlines ? "ON" : "OFF";
        else if (id == RETROPICOES_SETTINGS_OPTION_AUDIO) views[i].value = audio_enabled ? "ON" : "OFF";
        else if (id == RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE + 1u)
            views[i] = {"Controls","","VIEW KEYBOARD AND GAMEPAD CONTROLS"};
    }
    retropicoes_settings_view_t view{views,option_count,labels(),nullptr,0,0,0,
        static_cast<unsigned>(model) != saved_model || static_cast<unsigned>(scanlines) != saved_scanlines || static_cast<unsigned>(audio_enabled) != saved_audio};
    auto s = surface(); auto r = retropicoes_menu_nintendo_renderer();
    retropicoes_settings_menu_render(&r,&s,state,&view); compat_present();
}
void controls(bool test)
{
    retropicoes_settings_home_gate_t gate{}; retropicoes_settings_home_gate_init(&gate,0);
    for (;;) {
        compat_poll_input(); update_owner(nullptr);
        char buttons[64]; std::snprintf(buttons,sizeof(buttons),"Buttons: %s%s%s%s%s%s",
            compat_key_down(COMPAT_KEY_UP)?"UP ":"",compat_key_down(COMPAT_KEY_DOWN)?"DOWN ":"",
            compat_key_down(COMPAT_KEY_LEFT)?"LEFT ":"",compat_key_down(COMPAT_KEY_RIGHT)?"RIGHT ":"",
            compat_key_down(COMPAT_KEY_ACTION)?"A ":"",compat_key_down(COMPAT_KEY_JUMP)?"B ":"");
        const char *rows[] = {"D-Pad: machine arrow keys","A: SPACE  B: ENTER","Start: virtual keyboard","Keyboard: A send / B return","Menu/Home: Settings",test?buttons:"Virtual keyboard starts on ENTER"};
        auto s=surface(); auto r=retropicoes_menu_nintendo_renderer();
        retropicoes_menu_view_t v{test?"Controller Test":"TRS80 Controls",rows,6,0,0,6,labels(),nullptr};
        retropicoes_menu_render(&r,&s,&v); compat_present();
        auto a=map(mask(true));
        a=retropicoes_settings_home_gate_filter(&gate,a,mask(false)!=0,compat_ticks_ms(),100);
        if (a & (RETROPICOES_UI_CANCEL | RETROPICOES_UI_MENU)) return;
        compat_delay_ms(16);
    }
}
int settings_event(void *, retropicoes_settings_event_t e)
{
    if (e.type == RETROPICOES_SETTINGS_EVENT_CANCEL) { model = static_cast<trs80_model_t>(saved_model); scanlines = static_cast<int>(saved_scanlines); audio_enabled = static_cast<int>(saved_audio); return 0; }
    if (e.type == RETROPICOES_SETTINGS_EVENT_SAVE) {
        const uint8_t data[] = {'T','R','S',2,static_cast<uint8_t>(model),static_cast<uint8_t>(scanlines),static_cast<uint8_t>(audio_enabled)};
        auto *f = compat_file_open("/settings_TRS80.dat","wb");
        if (!f) return 1;
        bool ok = compat_file_write(data,1,sizeof(data),f) == sizeof(data);
        ok = compat_file_close(f) == 0 && ok;
        return !ok;
    }
    if (e.type == RETROPICOES_SETTINGS_EVENT_DEFAULT) { model = TRS80_MODEL_I; scanlines = 0; audio_enabled = 1; return 1; }
    if (e.item >= option_count) return 1;
    auto id = options[e.item];
    if (e.type == RETROPICOES_SETTINGS_EVENT_ACTIVATE || e.type == RETROPICOES_SETTINGS_EVENT_CHANGE) {
        if (id == RETROPICOES_SETTINGS_OPTION_SCANLINES) scanlines ^= 1;
        if (id == RETROPICOES_SETTINGS_OPTION_AUDIO) audio_enabled ^= 1;
        if (id == RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE) {
            model = model == TRS80_MODEL_I ? TRS80_MODEL_III : (model == TRS80_MODEL_III ? TRS80_MODEL_4 : TRS80_MODEL_I);
        }
        if (e.type == RETROPICOES_SETTINGS_EVENT_ACTIVATE) {
            if (id == RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST || id == RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE + 1u) {
                controls(id == RETROPICOES_SETTINGS_OPTION_CONTROLLER_TEST);
                retropicoes_settings_home_gate_init(&home_gate,0);
            }
            if (id == RETROPICOES_SETTINGS_OPTION_QUIT) { requested = RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER; return 0; }
            if (id == RETROPICOES_SETTINGS_OPTION_RESET) { requested = RETROPICOES_FRONTEND_SETTINGS_RESET; return 0; }
            if (id == RETROPICOES_SETTINGS_OPTION_RETURN_LOADER) { requested = RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER; return 0; }
        }
    }
    return 1;
}
void poll(void *) { compat_poll_input(); }
void delay(void *, unsigned ms) { compat_delay_ms(ms); }
retropicoes_ui_actions_t filter(void *, retropicoes_ui_actions_t a)
{
    return retropicoes_settings_home_gate_filter(&home_gate,a,mask(false) != 0,compat_ticks_ms(),100);
}
void settings()
{
    if (retropicoes_frontend_shell_open_settings(&shell) != RETROPICOES_MENU_RESULT_OK) return;
    saved_model = static_cast<unsigned>(model); saved_scanlines = static_cast<unsigned>(scanlines);
    saved_audio = static_cast<unsigned>(audio_enabled);
    requested = RETROPICOES_FRONTEND_SETTINGS_CANCEL;
    uint32_t caps = RETROPICOES_SETTINGS_CAP_RETURN_LOADER | RETROPICOES_SETTINGS_CAP_SCANLINES | RETROPICOES_SETTINGS_CAP_AUDIO | RETROPICOES_SETTINGS_CAP_CONTROLLER_TEST;
    if (shell.game_active) caps |= RETROPICOES_SETTINGS_CAP_QUIT | RETROPICOES_SETTINGS_CAP_RESET;
    const retropicoes_settings_option_id_t custom[] = {RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE,
        static_cast<retropicoes_settings_option_id_t>(RETROPICOES_SETTINGS_OPTION_CUSTOM_BASE+1)};
    option_count = retropicoes_settings_build_option_order(options,8,caps,custom,2);
    retropicoes_settings_platform_t p{nullptr,renderer,reset_owner,update_owner,controller,read_actions};
    retropicoes_settings_host_t h{nullptr,&p,option_count,12,nullptr,settings_render,poll,delay,filter,settings_event};
    retropicoes_settings_home_gate_init(&home_gate,0);
    retropicoes_settings_host_run(&h);
    if (retropicoes_frontend_shell_settings_action(&shell,requested) != RETROPICOES_MENU_RESULT_OK)
        retropicoes_frontend_shell_settings_action(&shell,RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER);
}
}
int main()
{
    if (compat_init() != 0) { std::puts("TRS80 FATAL: HW16 initialization failed"); return 1; }
    auto *expansion = static_cast<uint8_t *>(compat_large_malloc(0x10000u));
    if (!expansion || !retropicoes_pointer_in_psram(expansion) ||
        trs80_bind_expansion_ram(expansion,0x10000u) != TRS80_OK) {
        std::puts("TRS80 FATAL: expansion PSRAM allocation failed"); return 1;
    }
    uint8_t stored[7]{}; auto *f = compat_file_open("/settings_TRS80.dat","rb");
    if (f) {
        bool valid = compat_file_read(stored,1,sizeof(stored),f) == sizeof(stored);
        compat_file_close(f);
        if (valid && stored[0]=='T' && stored[1]=='R' && stored[2]=='S' && stored[3]==2 &&
            (stored[4]==1 || stored[4]==3 || stored[4]==4) && stored[5]<=1 && stored[6]<=1) { model=static_cast<trs80_model_t>(stored[4]); scanlines=stored[5]; audio_enabled=stored[6]; }
    }
    const retropicoes_frontend_lifecycle_t life{RETROPICOES_FRONTEND_LIFECYCLE_VERSION,sizeof(life),nullptr,launch,pause,resume,reset,stop,loader};
    if (!retropicoes_frontend_shell_init(&shell,&life)) return 1;
    std::puts("TRS80 READY: bios=/data/trs80/bios rom=/roms/TRS80 read-only JV1/JV3");
    for (;;) {
        scan();
        retropicoes_rom_source_t source{RETROPICOES_ROM_SOURCE_VERSION,sizeof(source),nullptr,"TRS-80 Game Selection","No compatible ROM found","Demo/Test Game (simulated)",count,item,path};
        retropicoes_rom_browser_open(&browser,&source,18);
        if (entry_count) {
            browser.list.selected = remembered_selection < entry_count ? remembered_selection : entry_count-1u;
            browser.list.first_visible = remembered_first <= browser.list.selected ? remembered_first : browser.list.selected;
        }
        retropicoes_menu_repeat_t repeat{};
        for (;;) {
            auto s = surface(); auto r = retropicoes_menu_nintendo_renderer();
            retropicoes_rom_browser_render(&browser,&r,&s,labels()); compat_present();
            compat_poll_input(); update_owner(nullptr);
            auto event = retropicoes_rom_browser_update(&browser,read_actions(nullptr,&repeat));
            remembered_selection=browser.list.selected; remembered_first=browser.list.first_visible;
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_SETTINGS) settings();
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_BACK) {
                if (std::strcmp(directory,"/roms/TRS80")) {
                    *std::strrchr(directory,'/') = 0;
                    if (depth) { --depth; remembered_selection=history[depth].selected; remembered_first=history[depth].first; }
                    break;
                }
                settings();
            }
            if (event.type == RETROPICOES_ROM_BROWSER_EVENT_OPEN) {
                if (event.item_kind == RETROPICOES_ROM_ITEM_DIRECTORY) {
                    size_t n = std::strlen(directory), add = std::strlen(entries[event.source_index].name);
                    if (n+add+2 >= sizeof(directory) || depth >= 16u) continue;
                    history[depth++]={remembered_selection,remembered_first}; remembered_selection=remembered_first=0;
                    std::snprintf(directory+n,sizeof(directory)-n,"/%s",entries[event.source_index].name);
                    break;
                }
                if (retropicoes_frontend_shell_launch(&shell,event.source_index,event.item_kind) != RETROPICOES_MENU_RESULT_OK) {
                    message("TRS80 Load Error",error_text); compat_delay_ms(2000); stop(nullptr); continue;
                }
                trs80_model_t running_model = active_model;
                trs80_virtual_keyboard_t vk{}; int release_gate = 1; unsigned key_frames = 0;
                uint64_t deadline = compat_ticks_us();
                while (shell.screen == RETROPICOES_FRONTEND_RUNNING) {
                    uint64_t frame_begin=compat_ticks_us();
                    compat_poll_input(); update_owner(nullptr); auto held = map(mask(false)); auto pressed = map(mask(true));
                    if (retropicoes_input_menu_pressed()) pressed |= RETROPICOES_UI_MENU;
                    if (release_gate) { if (!held) release_gate = 0; pressed = held = 0; }
                    if (vk.open) {
                        if (pressed & RETROPICOES_UI_MENU) {
                            trs80_vk_close(&vk); key_frames=0; settings(); release_gate=1;
                        }
                        auto e = trs80_vk_update(&vk,held);
                        if (e.type == TRS80_VK_SEND) {
                            trs80_release_keys(); trs80_key(e.key,1); if (e.shift) trs80_key(TRS80_MATRIX_SHIFT,1);
                            key_frames = 6; trs80_vk_close(&vk); release_gate = 1;
                            audio_resume(); perf.begin();
                        } else if (e.type == TRS80_VK_CLOSE) { release_gate = 1; audio_resume(); perf.begin(); }
                    } else if (pressed & RETROPICOES_UI_MENU) { settings(); release_gate = 1; }
                    else if (pressed & RETROPICOES_UI_AUX) { audio_pause(); trs80_release_keys(); trs80_vk_open(&vk,running_model); }
                    if (shell.screen != RETROPICOES_FRONTEND_RUNNING) break;
                    uint64_t input_end=compat_ticks_us();
                    if (!vk.open && !demo) {
                        if (key_frames) { if (!--key_frames) trs80_release_keys(); }
                        else {
                            trs80_release_keys();
                            const unsigned actions[] = {RETROPICOES_UI_UP,RETROPICOES_UI_DOWN,RETROPICOES_UI_LEFT,RETROPICOES_UI_RIGHT,RETROPICOES_UI_CONFIRM,RETROPICOES_UI_CANCEL};
                            const unsigned keys[] = {TRS80_MATRIX_UP,TRS80_MATRIX_DOWN,TRS80_MATRIX_LEFT,TRS80_MATRIX_RIGHT,TRS80_MATRIX_SPACE,TRS80_MATRIX_ENTER};
                            for (unsigned i=0;i<6;++i) if (held & actions[i]) trs80_key(keys[i],1);
                        }
                        if (trs80_run_cycles(trs80_status().clock_hz/60u) != TRS80_OK) {
                            message("TRS80 CPU Fault","Return to ROM browser"); compat_delay_ms(2000); retropicoes_frontend_shell_game_finished(&shell); break;
                        }
                    }
                    uint64_t cpu_end=compat_ticks_us(), draw_end=cpu_end, wait_us=0;
                    if (vk.open) { auto s2=surface(); trs80_vk_render(&vk,&s2,labels()); compat_present(); }
                    else if (demo) message("TRS80 Demo/Test Game","No real ROM loaded. MENU: Settings");
                    else { auto *fb=compat_framebuffer(); wait_us=compat_ticks_us()-cpu_end; trs80_render_video(fb->pixels,fb->stride,fb->width,fb->height,running_model,scanlines); draw_end=compat_ticks_us(); compat_present(); }
                    uint64_t video_end=compat_ticks_us();
                    deadline += 16667u; uint64_t now=compat_ticks_us();
                    if (deadline > now+1000u) compat_delay_ms(static_cast<uint32_t>((deadline-now)/1000u));
                    if (now > deadline+33334u) deadline=now;
                    if (!vk.open && !demo && input_end-frame_begin < 100000u)
                        perf.frame(cpu_end-input_end,draw_end-cpu_end-wait_us,input_end-frame_begin,video_end-draw_end,wait_us,video_end-frame_begin);
                }
                break;
            }
            compat_delay_ms(16);
        }
    }
}
