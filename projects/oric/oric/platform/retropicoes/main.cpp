/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#if defined(ORIC_TIMING_PROFILE)
#include "hardware/structs/m33.h"
#endif
#include "settings_host.h"
extern "C" {
#include "oric.h"
#include "adapter.h"
#include "compat.h"
#include "audio.h"
#include "input_backend.h"
#include "input_mapping.h"
#include "runtime.h"
#include "video_backend.h"
#include "frontend_shell.h"
}
#include "demo_tape.h"

namespace {
constexpr const char *root="/roms/ORIC";
constexpr const char *settings_path="/SAVES/ORIC/settings.dat";
constexpr char virtual_keys[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 \r\033\b";
constexpr unsigned max_entries=128;
struct Entry { char name[96]; bool directory; };
struct Stored { uint32_t magic,version; oric_options_t options; uint16_t reserved; uint32_t checksum; };
static_assert(sizeof(oric_options_t)==6,"stable settings encoding");
oric_t machine; // hot 64K address space and chips remain in SRAM
constexpr unsigned packed_row_bytes=ORIC_WIDTH*3/8;
constexpr unsigned packed_frame_bytes=packed_row_bytes*ORIC_HEIGHT;
uint8_t *frame_indices; // 3-bit raster indices in PSRAM; preserve SRAM heap for HSTX
uint8_t *bios,*tape;
size_t tape_size;
Entry *entries;
size_t entry_count;
char directory[256]="/roms/ORIC",status[80]={};
size_t parent_selection[16]; unsigned depth;
bool demo,live,audio_live;
oric_options_t options,working;
retropicoes_frontend_shell_t shell;
retropicoes_rom_browser_t browser;
retropicoes_menu_owner_t owner{RETROPICOES_MENU_NO_CONTROLLER};
retropicoes_controller_profile_t profile=RETROPICOES_CONTROLLER_UNKNOWN;
retropicoes_menu_repeat_t repeat;
retropicoes_settings_home_gate_t gate;
bool input_armed=false; uint32_t neutral_since=0;
uint32_t held_mask,pressed_mask; bool menu_edge;
uint64_t deadline;
unsigned typing_frame,send_frames; char send_char;
int16_t audio_storage[4096]; compat_audio_ring_t ring;
uint32_t underflow=0,dropped=0;
char uart_record[512]; unsigned uart_length,uart_offset;
uint64_t report_start,heartbeat_start,step_sum,draw_sum,present_sum,acquire_sum;
unsigned frame_count,peak_us,over_budget,fps10;
unsigned browser_frames;
retropicoes_video_stats_t previous_video{};
retropicoes_menu_surface_t surface;
retropicoes_settings_option_id_t setting_ids[12]; size_t setting_count;
int confirmation=-1;
bool save_selected=false;
retropicoes_frontend_settings_action_t requested;
#if defined(ORIC_TIMING_PROFILE)
bool profile_ready() {
    return (m33_hw->demcr&0x01000000u) && (m33_hw->dwt_ctrl&1u) && m33_hw->dwt_cyccnt;
}
unsigned core_cost(unsigned part,unsigned frames) {
    return profile_ready() && machine.profile_samples ? unsigned(uint64_t(machine.profile_cycles[part])*64u*1000000u/retropicoes_system_clock_hz()/frames):0;
}
unsigned ula_cost(unsigned frames) {
    return profile_ready() && machine.profile_lines ? unsigned(uint64_t(machine.profile_cycles[4])*1000000u/retropicoes_system_clock_hz()/frames):0;
}
unsigned profile_samples() { return profile_ready()?machine.profile_samples:0; }
unsigned profile_fast_hits() { return profile_ready()?machine.profile_via_fast*64u:0; }
unsigned profile_steady_hits() { return profile_ready()?machine.profile_via_steady*64u:0; }
unsigned profile_lazy_hits() { return profile_ready()?machine.profile_via_lazy*64u:0; }
unsigned profile_rendered_lines() { return profile_ready()?machine.profile_lines:0; }
void reset_profile() {
    std::memset(machine.profile_cycles,0,sizeof(machine.profile_cycles));
    machine.profile_samples=machine.profile_lines=0;
    machine.profile_via_fast=0;
    machine.profile_via_steady=0;
    machine.profile_via_lazy=0;
}
#else
void reset_profile() {}
#endif

void pump() {
    for(unsigned n=0;n<32 && uart_offset<uart_length && uart_is_writable(uart_default);++n)
        uart_putc_raw(uart_default,uart_record[uart_offset++]);
    if(uart_offset==uart_length) uart_offset=uart_length=0;
}
void heartbeat() {
    pump(); const auto now=compat_ticks_us();
    if(now-heartbeat_start>=10000000 && !uart_length) {
        retropicoes_video_stats_t video{};
        retropicoes_video_get_stats(&video);
        uart_length=std::snprintf(uart_record,sizeof(uart_record),
            "\r\nHEARTBEAT ORIC screen=%u frames=%lu tape=%u/%u psram=%u browser=%u display=%lu refresh=%lu video_drop=%lu\r\n",
            unsigned(shell.screen),(unsigned long)machine.frames,
            unsigned(machine.tape_offset),unsigned(machine.tape_size),retropicoes_psram_ready(),
            browser_frames,(unsigned long)video.displayed_frames,(unsigned long)video.hstx_frames,
            (unsigned long)video.dropped_frames);
        heartbeat_start=now;
    }
}
void notify(const char *message) { std::snprintf(status,sizeof(status),"%s",message); }
void poll() {
    compat_poll_input(); held_mask=pressed_mask=0;
    for(unsigned i=0;i<COMPAT_KEY_COUNT;++i) {
        if(compat_key_down(static_cast<compat_key_t>(i))) held_mask|=1u<<i;
        if(compat_key_pressed(static_cast<compat_key_t>(i))) pressed_mask|=1u<<i;
    }
    menu_edge=retropicoes_input_menu_pressed()!=0;
    heartbeat();
}
void update_owner(void *) {
    retropicoes_input_stats_t s{}; retropicoes_input_get_stats(&s);
    uint32_t actions[4];
    for(unsigned i=0;i<4;++i) actions[i]=oric_ui_actions(s.source_masks[i],profile,0);
    if(owner.active_controller<2 && !s.usb_name[owner.active_controller])
        retropicoes_menu_owner_disconnected(&owner,owner.active_controller);
    const auto p=retropicoes_menu_owner_update(&owner,actions,4);
    if(p<2) profile=retropicoes_menu_controller_profile_from_name(s.usb_name[p]);
    else if(p<4) profile=s.nes_pad_type[p-2]==RETROPICOES_NESPAD_SNES ?
        RETROPICOES_CONTROLLER_SNES:RETROPICOES_CONTROLLER_NES;
}
void reset_owner(void *) { retropicoes_menu_owner_reset(&owner); }
void disarm() { input_armed=false; neutral_since=0; retropicoes_menu_repeat_reset(&repeat); }
bool armed() {
    if(input_armed) return true;
    if(held_mask || menu_edge) neutral_since=0;
    else if(!neutral_since) neutral_since=compat_ticks_ms();
    else if(compat_ticks_ms()-neutral_since>=150) input_armed=true;
    return input_armed;
}
retropicoes_ui_actions_t read_actions(void *,retropicoes_menu_repeat_t *r) {
    const auto held=oric_ui_actions(held_mask,profile,0);
    const auto pressed=oric_ui_actions(pressed_mask,profile,menu_edge);
    return (pressed&~(RETROPICOES_UI_UP|RETROPICOES_UI_DOWN))|
        retropicoes_menu_repeat_update(r,held,pressed,compat_ticks_ms());
}
void get_renderer(void *,retropicoes_menu_renderer_t *r) { *r=retropicoes_menu_nintendo_renderer(); }
retropicoes_controller_profile_t get_profile(void *) { return profile; }
void begin_surface() {
    auto *fb=compat_framebuffer(); surface={fb->pixels,fb->stride,fb->width,fb->height};
}
void draw_status() {
    if(!status[0]) return;
    auto r=retropicoes_menu_nintendo_renderer();
    r.draw_text(&surface,0,208,status,0xf800,1,40,r.text_context);
}
bool join(char *out,size_t capacity,const char *base,const char *name) {
    if(!name[0] || std::strchr(name,'/') || std::strchr(name,'\\') || !std::strcmp(name,".") || !std::strcmp(name,"..")) return false;
    int n=std::snprintf(out,capacity,"%s/%s",base,name);
    return n>0 && size_t(n)<capacity;
}
int compare(const char *a,const char *b) {
    while(*a && *b) { int d=std::tolower(static_cast<unsigned char>(*a))-std::tolower(static_cast<unsigned char>(*b)); if(d) return d; ++a;++b; }
    return int(static_cast<unsigned char>(*a))-int(static_cast<unsigned char>(*b));
}
void scan() {
    entry_count=0; auto *dir=compat_dir_open(directory);
    if(!dir) { notify("NO ROM DIRECTORY - SIMULATED TEST"); return; }
    while(const char *name=compat_dir_read(dir)) {
        char path[256]; if(std::strlen(name)>=sizeof(Entry::name) || !join(path,sizeof(path),directory,name)) continue;
        bool is_dir=compat_path_type(path)==COMPAT_PATH_DIRECTORY;
        const char *dot=std::strrchr(name,'.');
        if(!is_dir && (!dot || compare(dot,".tap"))) continue;
        if(entry_count==max_entries) {notify("LIMIT 128 ENTRIES - USE SUBDIRECTORIES");break;}
        std::strcpy(entries[entry_count].name,name); entries[entry_count++].directory=is_dir;
    }
    compat_dir_close(dir);
    std::sort(entries,entries+entry_count,[](const Entry&a,const Entry&b){
        if(a.directory!=b.directory) return a.directory;
        return compare(a.name,b.name)<0;
    });
}
size_t source_count(void *) {return entry_count;}
int source_item(void *,size_t i,retropicoes_rom_item_view_t *out) {
    if(i>=entry_count) return 0;
    *out={entries[i].name,entries[i].directory?RETROPICOES_ROM_ITEM_DIRECTORY:RETROPICOES_ROM_ITEM_ROM};return 1;
}
const char *source_path(void *) {return directory;}
void audio_callback(int16_t *out,size_t count,void *) {
    const size_t got=compat_audio_ring_read(&ring,out,count);
    __atomic_fetch_add(&underflow,uint32_t(count-got),__ATOMIC_RELAXED);
}
void stop_audio() { if(audio_live) compat_audio_stop(); audio_live=false; }
bool start_audio() {
    compat_audio_ring_reset(&ring);
    int16_t silence[512]={};
    compat_audio_ring_write(&ring,silence,512); compat_audio_ring_write(&ring,silence,512);
    audio_live=compat_audio_start(ORIC_AUDIO_RATE,1,audio_callback,nullptr)==0;
    if(!audio_live) notify("AUDIO START FAILED");
    return audio_live;
}
bool read_bios() {
    const char *path=options.model?"/roms/ORIC/basic11b.rom":"/roms/ORIC/basic10.rom";
    auto *f=compat_file_open(path,"rb");
    if(!f) {notify(options.model?"MISSING /roms/ORIC/basic11b.rom":"MISSING /roms/ORIC/basic10.rom");return false;}
    bool ok=compat_file_size(f)==ORIC_ROM_SIZE && compat_file_read(bios,1,ORIC_ROM_SIZE,f)==ORIC_ROM_SIZE;
    if(compat_file_close(f)!=0) ok=false;
    if(!ok) notify("BASIC ROM MUST BE EXACTLY 16384 BYTES");
    return ok;
}
bool boot(bool play_audio=true) {
    if(demo) { std::memset(bios,0xea,ORIC_ROM_SIZE); bios[0]=0x4c; bios[1]=0; bios[2]=0xc0; bios[0x3ffc]=0; bios[0x3ffd]=0xc0; }
    else if(!read_bios()) return false;
    if(!frame_indices) frame_indices=static_cast<uint8_t*>(compat_large_malloc(packed_frame_bytes));
    if(!frame_indices){notify("HIRES VIDEO BUFFER ALLOCATION FAILED");return false;}
    if(oric_init(&machine,static_cast<oric_model_t>(options.model),bios,ORIC_ROM_SIZE,nullptr,0)!=ORIC_OK) return false;
    oric_set_packed_video(&machine,frame_indices);
    if(oric_attach_tape(&machine,tape,tape_size)!=ORIC_OK) {notify("INVALID OR UNSUPPORTED STANDARD TAP");return false;}
    if(demo && oric_load_binary(&machine,tape,tape_size)!=ORIC_OK) return false;
    typing_frame=0; send_frames=0; live=true; deadline=compat_ticks_us();
    report_start=deadline; frame_count=peak_us=over_budget=0; step_sum=draw_sum=present_sum=acquire_sum=0;
    retropicoes_video_get_stats(&previous_video); disarm();
    return !play_audio || start_audio();
}
retropicoes_menu_result_t stop(void *) {
    stop_audio(); oric_release_keys(&machine); live=false;
    compat_free(tape); tape=nullptr; tape_size=0; disarm();
    compat_free(frame_indices);frame_indices=nullptr;
    return RETROPICOES_MENU_RESULT_OK;
}
retropicoes_menu_result_t launch(void *,size_t index,retropicoes_rom_item_kind_t kind) {
    demo=kind==RETROPICOES_ROM_ITEM_DEMO;
    if(demo) {
        tape_size=sizeof(oric_demo_tape); tape=static_cast<uint8_t *>(compat_large_malloc(tape_size));
        if(tape) std::memcpy(tape,oric_demo_tape,tape_size);
    } else {
        char path[256]; if(index>=entry_count || !join(path,sizeof(path),directory,entries[index].name)) return RETROPICOES_MENU_RESULT_INVALID;
        auto *f=compat_file_open(path,"rb"); if(!f) {notify("TAP OPEN FAILED");return RETROPICOES_MENU_RESULT_FAILED;}
        long size=compat_file_size(f);
        if(size<=0 || size>ORIC_MAX_TAPE) {compat_file_close(f);notify("TAP SIZE MUST BE 1..1048576");return RETROPICOES_MENU_RESULT_FAILED;}
        tape_size=size; tape=static_cast<uint8_t *>(compat_large_malloc(tape_size));
        bool ok=tape && compat_file_read(tape,1,tape_size,f)==tape_size;
        if(compat_file_close(f)!=0) ok=false;
        if(!ok) {stop(nullptr);notify("TAP READ OR ALLOCATION FAILED");return RETROPICOES_MENU_RESULT_FAILED;}
        // Only our exact original fixtures are eligible for BIOS-free execution.
        demo=(tape_size==sizeof(oric_demo_tape) && !std::memcmp(tape,oric_demo_tape,tape_size)) ||
             (tape_size==sizeof(oric_hires_tape) && !std::memcmp(tape,oric_hires_tape,tape_size));
    }
    if(!tape || !boot()) {stop(nullptr);return RETROPICOES_MENU_RESULT_FAILED;}
    status[0]=0; return RETROPICOES_MENU_RESULT_OK;
}
retropicoes_menu_result_t pause(void *) {stop_audio();oric_release_keys(&machine);return RETROPICOES_MENU_RESULT_OK;}
retropicoes_menu_result_t resume(void *) {
    machine.video_dirty=true; // Settings overwrote the shared scanout buffer
    deadline=report_start=compat_ticks_us();
    frame_count=peak_us=over_budget=0;step_sum=draw_sum=present_sum=acquire_sum=0;
    retropicoes_video_get_stats(&previous_video);disarm();
    return start_audio()?RETROPICOES_MENU_RESULT_OK:RETROPICOES_MENU_RESULT_FAILED;
}
retropicoes_menu_result_t reset(void *) {stop_audio();return boot(false)?RETROPICOES_MENU_RESULT_OK:RETROPICOES_MENU_RESULT_FAILED;}
retropicoes_menu_result_t loader(void *) {stop(nullptr);retropicoes_return_to_bootloader();return RETROPICOES_MENU_RESULT_FAILED;}
void load_options() {
    oric_options_default(&options); Stored s{}; auto *f=compat_file_open(settings_path,"rb"); if(!f)return;
    const bool ok=compat_file_size(f)==sizeof(s) && compat_file_read(&s,1,sizeof(s),f)==sizeof(s);
    compat_file_close(f);
    if(ok && s.magic==0x4349524f && s.version==1 && oric_options_valid(&s.options) && s.checksum==oric_options_checksum(&s.options)) options=s.options;
    else notify("INVALID SETTINGS - USING DEFAULTS");
}
bool save_options() {
    compat_dir_create("/SAVES"); compat_dir_create("/SAVES/ORIC");
    auto *f=compat_file_open(settings_path,"wb"); if(!f)return false;
    Stored s{0x4349524f,1,working,0,oric_options_checksum(&working)};
    bool ok=compat_file_write(&s,1,sizeof(s),f)==sizeof(s);
    if(compat_file_close(f)!=0)ok=false;
    return ok;
}
const char *key_name(unsigned key,char *out) {
    const char c=virtual_keys[key];
    if(c==' ')return "SPACE";
    if(c=='\r')return "RETURN";
    if(c==27)return "ESC";
    if(c==8)return "BACKSPACE";
    out[0]=c;out[1]=0;return out;
}
void render_settings(void *,const retropicoes_settings_menu_state_t *state) {
    save_selected=state->on_action_row && state->action==RETROPICOES_SETTINGS_ACTION_SAVE;
    if(confirmation>=0 && (state->on_action_row || int(state->list.selected)!=confirmation))confirmation=-1;
    retropicoes_settings_item_view_t items[12]{}; char key[2];
    for(size_t i=0;i<setting_count;++i) {
        const auto id=setting_ids[i]; items[i].label=retropicoes_settings_common_label(id);
        items[i].help=retropicoes_settings_common_help(id); items[i].value="";
        switch(static_cast<unsigned>(id)) {
        case RETROPICOES_SETTINGS_OPTION_AUDIO:items[i].value=working.audio?"ON":"OFF";break;
        case RETROPICOES_SETTINGS_OPTION_SCANLINES:items[i].value=working.scanlines?"ON":"OFF";break;
        case RETROPICOES_SETTINGS_OPTION_FPS:items[i].value=working.fps?"ON":"OFF";break;
        case 256:items[i]={"Machine",working.model?"ATMOS":"ORIC-1","APPLIES AT NEXT LAUNCH OR RESET"};break;
        case 257:items[i]={"Direction keys",working.qaop?"QAOP":"ARROWS","A=SPACE B=CTRL START=RETURN BACK=ESC"};break;
        case 258:items[i]={"Virtual key",key_name(working.key,key),"LEFT/RIGHT CHOOSES ONE GUEST KEY"};break;
        case 259:items[i]={"Send key","","SEND SELECTED KEY AND RESUME"};break;
        default:break;
        }
        if(confirmation==int(i))items[i].help="PRESS A AGAIN TO CONFIRM; B CANCELS";
    }
    retropicoes_settings_view_t view{items,setting_count,retropicoes_menu_controller_labels(profile),nullptr,0,0,0,std::memcmp(&working,&options,sizeof(options))!=0};
    auto r=retropicoes_menu_nintendo_renderer();
    retropicoes_settings_menu_render(&r,&surface,state,&view);draw_status();compat_present();
}
int settings_event(void *,retropicoes_settings_event_t e) {
    if(e.type==RETROPICOES_SETTINGS_EVENT_CANCEL) {requested=RETROPICOES_FRONTEND_SETTINGS_CANCEL;return 0;}
    if(e.type==RETROPICOES_SETTINGS_EVENT_DEFAULT) {oric_options_default(&working);confirmation=-1;return 1;}
    if(e.type==RETROPICOES_SETTINGS_EVENT_SAVE) {
        options=working;requested=RETROPICOES_FRONTEND_SETTINGS_RESUME;return 0;
    }
    if(e.type==RETROPICOES_SETTINGS_EVENT_REDRAW) {confirmation=-1;return 1;}
    if(e.type!=RETROPICOES_SETTINGS_EVENT_CHANGE && e.type!=RETROPICOES_SETTINGS_EVENT_ACTIVATE)return 1;
    if(e.item>=setting_count)return 1;
    auto id=setting_ids[e.item];
    if(id==RETROPICOES_SETTINGS_OPTION_RESET || id==RETROPICOES_SETTINGS_OPTION_RETURN_LOADER || id==RETROPICOES_SETTINGS_OPTION_QUIT) {
        if(e.type!=RETROPICOES_SETTINGS_EVENT_ACTIVATE)return 1;
        if(confirmation!=int(e.item)){confirmation=int(e.item);return 1;}
        requested=id==RETROPICOES_SETTINGS_OPTION_RESET?RETROPICOES_FRONTEND_SETTINGS_RESET:
            id==RETROPICOES_SETTINGS_OPTION_QUIT?RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER:RETROPICOES_FRONTEND_SETTINGS_RETURN_LOADER;return 0;
    }
    confirmation=-1;
    switch(static_cast<unsigned>(id)) {
    case RETROPICOES_SETTINGS_OPTION_AUDIO:working.audio^=1;break;
    case RETROPICOES_SETTINGS_OPTION_SCANLINES:working.scanlines^=1;break;
    case RETROPICOES_SETTINGS_OPTION_FPS:working.fps^=1;break;
    case 256:working.model^=1;break;
    case 257:working.qaop^=1;break;
    case 258:working.key=uint8_t((working.key+40+(e.delta<0?-1:1))%40);break;
    case 259:if(e.type==RETROPICOES_SETTINGS_EVENT_ACTIVATE){send_char=virtual_keys[working.key];send_frames=5;requested=RETROPICOES_FRONTEND_SETTINGS_RESUME;return 0;}break;
    default:break;
    }
    return 1;
}
void open_settings() {
    if(retropicoes_frontend_shell_open_settings(&shell)!=RETROPICOES_MENU_RESULT_OK)return;
    working=options;confirmation=-1;status[0]=0;requested=RETROPICOES_FRONTEND_SETTINGS_CANCEL;
    const retropicoes_settings_option_id_t custom[]={static_cast<retropicoes_settings_option_id_t>(256),static_cast<retropicoes_settings_option_id_t>(257),static_cast<retropicoes_settings_option_id_t>(258),static_cast<retropicoes_settings_option_id_t>(259)};
    uint32_t caps=RETROPICOES_SETTINGS_CAP_AUDIO|RETROPICOES_SETTINGS_CAP_SCANLINES|RETROPICOES_SETTINGS_CAP_FPS|RETROPICOES_SETTINGS_CAP_RETURN_LOADER;
    if(live)caps|=RETROPICOES_SETTINGS_CAP_QUIT|RETROPICOES_SETTINGS_CAP_RESET;
    setting_count=retropicoes_settings_build_option_order(setting_ids,12,caps,custom,live?4:3);
    retropicoes_settings_home_gate_init(&gate,0);
    const retropicoes_settings_platform_t platform{nullptr,get_renderer,reset_owner,update_owner,get_profile,read_actions};
    const retropicoes_settings_host_t host{nullptr,&platform,setting_count,12,
        [](void*){begin_surface();},render_settings,[](void*){poll();},
        [](void*,unsigned ms){compat_delay_ms(ms);},
        [](void*,retropicoes_ui_actions_t a){
            a=retropicoes_settings_home_gate_filter(&gate,a,held_mask||menu_edge,compat_ticks_ms(),150);
            // Intercept before the common state closes, so a failed SD write
            // leaves a usable Settings page and the original options intact.
            if(save_selected && (a&RETROPICOES_UI_CONFIRM) &&
               !(a&(RETROPICOES_UI_CANCEL|RETROPICOES_UI_MENU|RETROPICOES_UI_UP|RETROPICOES_UI_DOWN|RETROPICOES_UI_LEFT|RETROPICOES_UI_RIGHT)) && !save_options()) {
                notify("SETTINGS WRITE FAILED - B CANCELS");return retropicoes_ui_actions_t(0);
            }
            return a;
        },settings_event};
    retropicoes_settings_host_run(&host);
    if(retropicoes_frontend_shell_settings_action(&shell,requested)!=RETROPICOES_MENU_RESULT_OK) {
        notify("LIFECYCLE ACTION FAILED - RETURNED TO LIST");
        if(shell.screen==RETROPICOES_FRONTEND_SETTINGS)retropicoes_frontend_shell_settings_action(&shell,RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER);
    }
    disarm();
}
void guest_key(char c) {bool shift=false;const int k=oric_ascii_key(c,&shift);if(k>=0)oric_key(&machine,k,true);if(shift)oric_key(&machine,ORIC_KEY_SHIFT,true);}
void input_frame() {
    oric_release_keys(&machine);
    if(!demo && typing_frame<240) {
        if(typing_frame==150) {
            oric_tape_block_t block{};
            if(oric_tape_block(tape,tape_size,0,&block)==ORIC_OK &&
               block.type==0x80 && block.autorun && block.next_offset==tape_size &&
               oric_load_binary(&machine,tape,tape_size)==ORIC_OK) {
                std::printf("ORIC FASTLOAD bytes=%u start=%04x\n",unsigned(tape_size),unsigned(block.start));
                typing_frame=240;
                return;
            }
        }
        constexpr char command[]="CLOAD\"\"\r";
        if(typing_frame>=150){unsigned n=(typing_frame-150)/10;if(n<sizeof(command)-1 && (typing_frame-150)%10<5)guest_key(command[n]);}
        ++typing_frame;return;
    }
    if(!armed())return;
    const unsigned keys[]={ORIC_KEY_LEFT,ORIC_KEY_RIGHT,ORIC_KEY_UP,ORIC_KEY_DOWN};
    const char qaop[]="OPQA";
    for(unsigned i=0;i<4;++i)if(held_mask&(1u<<i)){if(options.qaop)guest_key(qaop[i]);else oric_key(&machine,keys[i],true);}
    const auto face=oric_ui_actions(held_mask,profile,0);
    if(face&RETROPICOES_UI_CONFIRM)oric_key(&machine,ORIC_KEY_SPACE,true);
    if(face&RETROPICOES_UI_CANCEL)oric_key(&machine,ORIC_KEY_CTRL,true);
    if(held_mask&(1u<<COMPAT_KEY_START))oric_key(&machine,ORIC_KEY_RETURN,true);
    if(held_mask&(1u<<COMPAT_KEY_BACK))oric_key(&machine,ORIC_KEY_ESCAPE,true);
    if(send_frames){guest_key(send_char);--send_frames;}
}
COMPAT_HOT(oric_draw_packed_line) void draw_packed_line(unsigned y) {
    const uint16_t colors[]={0,0xf800,0x07e0,0xffe0,0x001f,0xf81f,0x07ff,0xffff};
    auto *row=surface.pixels+(y+8)*surface.stride+40;
    const auto *src=frame_indices+y*packed_row_bytes;
    const bool dim=options.scanlines && (y&1);
    for(unsigned group=0;group<ORIC_WIDTH/8;++group) {
        uint32_t bits=uint32_t(src[group*3])|
                      (uint32_t(src[group*3+1])<<8)|
                      (uint32_t(src[group*3+2])<<16);
        for(unsigned bit=0;bit<8;++bit) {
            uint16_t color=colors[(bits>>(bit*3))&7];
            row[group*8+bit]=dim?(color>>1)&0x7bef:color;
        }
    }
}
void game_frame() {
    const uint64_t start=compat_ticks_us(); input_frame();
    const uint64_t prepared=compat_ticks_us();
    // Emulate into a packed PSRAM frame before claiming the inactive scanout
    // buffer. The previous publication can reach VSYNC while this work runs.
    oric_run_frame(&machine);const uint64_t stepped=compat_ticks_us();
    if(!options.audio)std::memset(machine.audio,0,machine.audio_count*sizeof(int16_t));
    dropped+=machine.audio_count-compat_audio_ring_write(&ring,machine.audio,machine.audio_count);
    uint64_t drawn=compat_ticks_us(),presented=drawn;
    if(machine.video_changed || options.fps) {
        const uint64_t before_acquire=compat_ticks_us();begin_surface();
        acquire_sum+=compat_ticks_us()-before_acquire;
        for(unsigned y=0;y<240;++y) {
            auto *row=surface.pixels+y*surface.stride;
            if(y<8 || y>=232)std::fill(row,row+320,0);
            else {std::fill(row,row+40,0);std::fill(row+280,row+320,0);}
        }
        for(unsigned y=0;y<ORIC_HEIGHT;++y)
            draw_packed_line(y);
        if(options.fps){char text[24];std::snprintf(text,sizeof(text),"%u.%u FPS",fps10/10,fps10%10);auto r=retropicoes_menu_nintendo_renderer();r.draw_text(&surface,0,0,text,0xffff,1,20,nullptr);}
        drawn=compat_ticks_us();compat_present();presented=compat_ticks_us();
    }
    step_sum+=stepped-prepared;draw_sum+=(prepared-start)+(drawn-stepped);present_sum+=presented-drawn;
    const unsigned used=presented-start;peak_us=std::max(peak_us,used);if(used>machine.frame_cycles)++over_budget;++frame_count;
    deadline+=machine.frame_cycles;
    if(presented>deadline+machine.frame_cycles)deadline=presented;
    for(;;){const auto time=compat_ticks_us();if(time>=deadline)break;heartbeat();if(deadline-time>1500)compat_delay_ms(1);}
    const uint64_t now=compat_ticks_us(),elapsed=now-report_start;
    if(elapsed>=2000000){
        retropicoes_video_stats_t vs{};retropicoes_video_get_stats(&vs);
        fps10=unsigned(uint64_t(frame_count)*10000000/elapsed);
        const unsigned target10=unsigned((10000000ull+machine.frame_cycles/2u)/machine.frame_cycles);
        if(!uart_length) {
            uart_length=std::snprintf(uart_record,sizeof(uart_record),
            "\r\nPERF ORIC fps=%u.%u target_fps=%u.%u step=%u draw=%u acquire=%u present=%u peak=%u over=%u/%u audio_under=%lu audio_drop=%lu video_drop=%lu display=%u refresh=%u resync=%lu\r\n",
            fps10/10,fps10%10,target10/10,target10%10,unsigned(step_sum/frame_count),unsigned(draw_sum/frame_count),unsigned(acquire_sum/frame_count),unsigned(present_sum/frame_count),peak_us,over_budget,frame_count,
            (unsigned long)__atomic_load_n(&underflow,__ATOMIC_RELAXED),(unsigned long)dropped,(unsigned long)(vs.dropped_frames-previous_video.dropped_frames),
            unsigned(uint64_t(vs.displayed_frames-previous_video.displayed_frames)*1000000/elapsed),
            unsigned(uint64_t(vs.hstx_frames-previous_video.hstx_frames)*1000000/elapsed),
            (unsigned long)(vs.resync_count-previous_video.resync_count));
#if defined(ORIC_TIMING_PROFILE)
            if(uart_length<sizeof(uart_record)) uart_length+=std::snprintf(uart_record+uart_length,sizeof(uart_record)-uart_length,
            "PERF CORE cpu=%u prep=%u via=%u bus=%u ay=%u ula=%u profile=%u fast=%u steady=%u lazy=%u render=%u\r\n",
            core_cost(0,frame_count),core_cost(5,frame_count),core_cost(1,frame_count),core_cost(2,frame_count),
            core_cost(3,frame_count),ula_cost(frame_count),profile_samples(),profile_fast_hits(),profile_steady_hits(),profile_lazy_hits(),profile_rendered_lines());
#endif
        }
        previous_video=vs;report_start=now;frame_count=peak_us=over_budget=0;step_sum=draw_sum=present_sum=acquire_sum=0;
        reset_profile();
    }
}
}
int main() {
    if(compat_init()!=0)return 1;
#if defined(ORIC_TIMING_PROFILE)
    m33_hw->demcr|=0x01000000u; /* enable DWT cycle counter on core 0 */
    m33_hw->dwt_cyccnt=0;
    m33_hw->dwt_ctrl|=1u;
#endif
    std::printf("ORIC candidate frontend=%u HW_CONFIG=16 SRAM core=%u PSRAM=%u\n",RETROPICOES_FRONTEND_CONTRACT_VERSION,unsigned(sizeof(machine)),retropicoes_psram_ready());
    std::printf("ORIC BOOT alloc begin\n");
    bios=static_cast<uint8_t*>(compat_large_malloc(ORIC_ROM_SIZE));
    entries=static_cast<Entry*>(compat_large_calloc(max_entries,sizeof(Entry)));
    if(!bios || !entries){std::printf("ORIC allocation failed\n");return 1;}
    std::printf("ORIC BOOT alloc ok\n");
    compat_audio_ring_init(&ring,audio_storage,4096);
    load_options();std::printf("ORIC BOOT settings ok\n");
    scan();std::printf("ORIC BOOT scan ok entries=%u\n",unsigned(entry_count));
    const retropicoes_rom_source_t source{RETROPICOES_ROM_SOURCE_VERSION,sizeof(source),nullptr,"ORIC-1 / ATMOS","NO TAP FILES","SIMULATED TEST (NO BASIC ROM)",source_count,source_item,source_path};
    const int browser_opened=retropicoes_rom_browser_open(&browser,&source,18);
    std::printf("ORIC BOOT browser open=%d demo=%d count=%u\n",browser_opened,int(browser.showing_demo),unsigned(browser.source_count));
    const retropicoes_frontend_lifecycle_t life{RETROPICOES_FRONTEND_LIFECYCLE_VERSION,sizeof(life),nullptr,launch,pause,resume,reset,stop,loader};
    const int shell_opened=retropicoes_frontend_shell_init(&shell,&life);disarm();
    std::printf("ORIC BOOT shell open=%d screen=%u\n",shell_opened,unsigned(shell.screen));
    bool first_browser_frame=true;
    for(;;) {
        poll();update_owner(nullptr);
        if(shell.screen==RETROPICOES_FRONTEND_RUNNING) {
            if(armed() && menu_edge)open_settings();else game_frame();continue;
        }
        if(shell.screen==RETROPICOES_FRONTEND_STOPPED){compat_delay_ms(10);continue;}
        auto event=retropicoes_rom_browser_update(&browser,armed()?read_actions(nullptr,&repeat):0);
        if(event.type==RETROPICOES_ROM_BROWSER_EVENT_SETTINGS)open_settings();
        if(event.type==RETROPICOES_ROM_BROWSER_EVENT_BACK && !depth)open_settings();
        else if(event.type==RETROPICOES_ROM_BROWSER_EVENT_BACK && depth) {
            char *slash=std::strrchr(directory,'/');if(slash)*slash=0;scan();retropicoes_rom_browser_refresh(&browser);
            browser.list.selected=parent_selection[--depth];if(browser.list.selected>=browser.source_count)browser.list.selected=0;
            browser.list.first_visible=(browser.list.selected/browser.list.page_rows)*browser.list.page_rows;disarm();
        }
        if(event.type==RETROPICOES_ROM_BROWSER_EVENT_OPEN) {
            if(event.item_kind==RETROPICOES_ROM_ITEM_DIRECTORY) {
                char path[256];if(depth<16 && join(path,sizeof(path),directory,entries[event.source_index].name)) {
                    parent_selection[depth++]=browser.list.selected;std::strcpy(directory,path);status[0]=0;scan();retropicoes_rom_browser_refresh(&browser);browser.list.selected=browser.list.first_visible=0;disarm();
                } else notify("DIRECTORY DEPTH OR PATH LIMIT");
            } else {retropicoes_frontend_shell_launch(&shell,event.source_index,event.item_kind);disarm();}
        }
        if(shell.screen==RETROPICOES_FRONTEND_ROM_BROWSER){
            if(first_browser_frame)std::printf("ORIC BOOT render begin\n");
            begin_surface();auto r=retropicoes_menu_nintendo_renderer();
            const int rendered=retropicoes_rom_browser_render(&browser,&r,&surface,retropicoes_menu_controller_labels(profile));draw_status();
            unsigned nonzero=0;
            if(first_browser_frame)for(unsigned i=0;i<surface.width*surface.height;i+=32)
                nonzero+=surface.pixels[(i/surface.width)*surface.stride+(i%surface.width)]!=0;
            compat_present();++browser_frames;
            if(first_browser_frame){std::printf("ORIC BOOT render rc=%d sampled_nonzero=%u\n",rendered,nonzero);first_browser_frame=false;}
            compat_delay_ms(16);
        }
    }
}
