/* SPDX-License-Identifier: GPL-3.0-or-later
 * Compile the real target adapter against deterministic I/O mocks. */
#define main oric_target_main
#include "../platform/retropicoes/main.cpp"
#undef main
#include <cassert>
#include <cstdlib>
#include <vector>

struct compat_file { size_t offset; bool write; };
static std::vector<uint8_t> stored;
static bool write_failure;
static bool mock_audio;
static unsigned audio_starts,audio_stops,poll_index;
static uint64_t clock_us;
static uint32_t mock_mask,mock_pressed;
static bool mock_menu;
static std::vector<uint32_t> reports;
static uint16_t framebuffer_pixels[320*240];
static unsigned present_count;
extern "C" {
int compat_init(){return 0;}
compat_framebuffer_t *compat_framebuffer(){static compat_framebuffer_t f{framebuffer_pixels,320,320,240};return &f;}
void compat_present(){++present_count;}
void compat_poll_input(){uint32_t next=poll_index<reports.size()?reports[poll_index++]:0;mock_pressed=next&~mock_mask;mock_mask=next;mock_menu=false;assert(poll_index<500);}
int compat_key_down(compat_key_t k){return !!(mock_mask&(1u<<k));}
int compat_key_pressed(compat_key_t k){return !!(mock_pressed&(1u<<k));}
int compat_audio_start(unsigned hz,unsigned channels,compat_audio_callback_t,void*){assert(hz==22050 && channels==1 && !mock_audio);mock_audio=true;++audio_starts;return 0;}
void compat_audio_stop(){assert(mock_audio);mock_audio=false;++audio_stops;}
compat_file_t *compat_file_open(const char *path,const char *mode){if(std::strcmp(path,settings_path))return nullptr;if(mode[0]=='w'){if(write_failure)return nullptr;stored.clear();return new compat_file{0,true};}if(stored.empty())return nullptr;return new compat_file{0,false};}
size_t compat_file_read(void *p,size_t sz,size_t n,compat_file_t *f){size_t bytes=std::min(sz*n,stored.size()-f->offset);std::memcpy(p,stored.data()+f->offset,bytes);f->offset+=bytes;return bytes/sz;}
size_t compat_file_write(const void *p,size_t sz,size_t n,compat_file_t*){auto *data=static_cast<const uint8_t*>(p);stored.assign(data,data+sz*n);return n;}
long compat_file_size(compat_file_t*){return long(stored.size());}
int compat_file_close(compat_file_t*f){delete f;return 0;}
int compat_dir_create(const char*){return 1;}
compat_dir_t *compat_dir_open(const char*){return nullptr;}
const char *compat_dir_read(compat_dir_t*){return nullptr;}
void compat_dir_close(compat_dir_t*){}
compat_path_type_t compat_path_type(const char*){return COMPAT_PATH_MISSING;}
uint32_t compat_ticks_ms(){return uint32_t(clock_us/1000);}
uint64_t compat_ticks_us(){return clock_us++;}
void compat_delay_ms(uint32_t ms){clock_us+=uint64_t(ms)*1000;}
void *compat_large_malloc(size_t n){return std::malloc(n);}
void *compat_large_calloc(size_t n,size_t sz){return std::calloc(n,sz);}
void compat_free(void*p){std::free(p);}
int retropicoes_psram_ready(){return 1;}
void retropicoes_input_get_stats(retropicoes_input_stats_t*s){*s={};s->source_masks[2]=mock_mask;s->nes_pad_type[0]=RETROPICOES_NESPAD_NES;}
int retropicoes_input_menu_pressed(){return mock_menu;}
void retropicoes_video_get_stats(retropicoes_video_stats_t*s){*s={};}
void retropicoes_return_to_bootloader(){}
}
static void sequence(std::initializer_list<uint32_t> events){
    reports.assign(12,0); // consume held opening button and 150ms release gate
    for(auto e:events){reports.push_back(e);reports.push_back(0);}
    poll_index=0;mock_mask=mock_pressed=0;
}
int main(){
    begin_surface();bios=static_cast<uint8_t*>(std::malloc(ORIC_ROM_SIZE));
    compat_audio_ring_init(&ring,audio_storage,4096);oric_options_default(&options);profile=RETROPICOES_CONTROLLER_NES;
    const retropicoes_frontend_lifecycle_t life{1,sizeof(life),nullptr,launch,pause,resume,reset,stop,loader};
    assert(retropicoes_frontend_shell_init(&shell,&life));
    assert(retropicoes_frontend_shell_launch(&shell,RETROPICOES_ROM_BROWSER_DEMO_INDEX,RETROPICOES_ROM_ITEM_DEMO)==0 && live && mock_audio);
    game_frame();
    assert(present_count==1);
    const uint16_t palette[]={0,0xf800,0x07e0,0xffe0,0x001f,0xf81f,0x07ff,0xffff};
    assert(frame_indices);
    for(unsigned y=0;y<ORIC_HEIGHT;++y)
        for(unsigned x=0;x<ORIC_WIDTH;++x) {
            const auto *src=frame_indices+y*packed_row_bytes+(x/8)*3;
            const uint32_t bits=uint32_t(src[0])|(uint32_t(src[1])<<8)|(uint32_t(src[2])<<16);
            assert(framebuffer_pixels[(y+8)*320+x+40]==palette[(bits>>((x&7)*3))&7]);
        }
    for(unsigned i=0;i<10;++i)oric_run_frame(&machine);
    assert(machine.memory[0x20]==18);
    const auto frames_before=machine.frames;
    const uint32_t up=1u<<COMPAT_KEY_UP,a=1u<<COMPAT_KEY_ACTION,b=1u<<COMPAT_KEY_JUMP;
    // SAVE failure must keep host navigation usable, B must still cancel.
    write_failure=true;sequence({up,a,b});open_settings();
    assert(shell.screen==RETROPICOES_FRONTEND_RUNNING && machine.frames==frames_before && mock_audio);
    assert(stored.empty() && std::strstr(status,"WRITE FAILED"));
    write_failure=false;sequence({up,a});open_settings();
    assert(shell.screen==RETROPICOES_FRONTEND_RUNNING && stored.size()==sizeof(Stored));
    working=options;working.qaop=1;working.audio=0;assert(save_options());
    load_options();assert(options.qaop && !options.audio);
    stored.back()^=1;load_options();assert(!options.qaop && options.audio);
    // Reset stops the old consumer before rebuilding state and re-primes once.
    assert(retropicoes_frontend_shell_open_settings(&shell)==0);
    unsigned starts_before=audio_starts;
    assert(retropicoes_frontend_shell_settings_action(&shell,RETROPICOES_FRONTEND_SETTINGS_RESET)==0);
    assert(audio_starts==starts_before+1 && mock_audio && machine.frames==0);
    // Quit frees borrowed tape only after audio has stopped; relaunch works.
    assert(retropicoes_frontend_shell_open_settings(&shell)==0);
    assert(retropicoes_frontend_shell_settings_action(&shell,RETROPICOES_FRONTEND_SETTINGS_ROM_BROWSER)==0 && !tape && !live && !mock_audio);
    assert(retropicoes_frontend_shell_launch(&shell,0,RETROPICOES_ROM_ITEM_DEMO)==0);
    // A static guest frame reuses the published image; Settings must repaint.
    machine.memory[0xc000]=0x4c;machine.memory[0xc001]=0x00;machine.memory[0xc002]=0xc0;
    oric_jump(&machine,0xc000);machine.video_dirty=false;machine.video_full_dirty=false;
    std::memset(machine.video_dirty_lines,0,sizeof(machine.video_dirty_lines));
    machine.last_render_blink=uint8_t(machine.frames&16u);
    const unsigned published=present_count;
    game_frame();assert(present_count==published && !machine.video_changed);
    machine.video_full_dirty=true;
    game_frame();assert(present_count==published+1 && machine.video_changed);
    stop(nullptr);assert(audio_starts==audio_stops);
    std::free(bios);
    return 0;
}
