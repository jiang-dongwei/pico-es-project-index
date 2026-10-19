/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "trs80.h"
#include "video_adapter.h"
#include "media_model.h"
#include <string.h>
int trs80_render_reference(uint16_t *,size_t,unsigned,unsigned,trs80_model_t,int);
static uint16_t reference[512u * 288u];
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { printf("line %d: %s\n",__LINE__,#x); return 1; } } while (0)
static uint16_t pixels[512u * 288u];
static unsigned samples, nonzero;
static int16_t last_sample;
static void sample(int16_t value, void *context)
{
    (void)context; ++samples; if (value) ++nonzero; last_sample=value;
}
int main(void)
{
    CHECK(trs80_media_model("/roms/TRS80/Model_4",TRS80_MODEL_I)==TRS80_MODEL_4);
    CHECK(trs80_media_model("/roms/TRS80/Model_III/sub",TRS80_MODEL_I)==TRS80_MODEL_III);
    CHECK(trs80_media_model("/roms/TRS80/Model_I",TRS80_MODEL_4)==TRS80_MODEL_I);
    CHECK(trs80_media_model("/roms/TRS80/Model_4_extra",TRS80_MODEL_III)==TRS80_MODEL_III);
    CHECK(trs80_media_model("/roms/TRS80",TRS80_MODEL_III)==TRS80_MODEL_III);
    const unsigned dimensions[][2]={{320,240},{512,192},{257,193},{79,288}};
    const trs80_model_t models[]={TRS80_MODEL_I,TRS80_MODEL_III,TRS80_MODEL_4};
    for (unsigned m=0;m<3u;++m) for (unsigned mode=0;mode<32u;++mode) {
        CHECK(trs80_init(models[m])==TRS80_OK);
        trs80_out(0xffu,(uint8_t)((mode & 1u) ? (models[m]==TRS80_MODEL_I ? 8u : 4u) : 0u));
        if (models[m]!=TRS80_MODEL_I) trs80_out(0xecu,(uint8_t)(((mode & 1u)?4u:0u) | ((mode & 2u)?8u:0u)));
        for (unsigned i=0;i<2048u;++i) trs80_write((uint16_t)(0x3c00u+i),(uint8_t)(i*73u+mode*19u));
        if (models[m]==TRS80_MODEL_4) {
            trs80_out(0x84u,2u);
            for (unsigned i=0;i<2048u;++i) trs80_write((uint16_t)(0xf800u+i),(uint8_t)(i*29u+mode*7u));
            trs80_out(0x84u,(uint8_t)(((mode&4u)?4u:0u)|((mode&8u)?8u:0u)|((mode&16u)?0x80u:0u)));
        }
        for (unsigned d=0;d<4u;++d) for (int scan=0;scan<2;++scan) {
            memset(pixels,0x55,sizeof(pixels)); memset(reference,0x55,sizeof(reference));
            CHECK(trs80_render_video(pixels,512,dimensions[d][0],dimensions[d][1],models[m],scan));
            CHECK(trs80_render_reference(reference,512,dimensions[d][0],dimensions[d][1],models[m],scan));
            CHECK(!memcmp(pixels,reference,sizeof(pixels)));
        }
    }
    CHECK(trs80_init(TRS80_MODEL_I) == TRS80_OK);
    for (unsigned bit=0;bit<6u;++bit) {
        trs80_write(0x3c00u,(uint8_t)(0x80u | (1u<<bit)));
        CHECK(trs80_render_video(pixels,512,512,192,TRS80_MODEL_I,0));
        for (unsigned y=0;y<12u;++y) for (unsigned x=0;x<8u;++x)
            CHECK((pixels[y*512u+x] != 0u) == ((y/4u)*2u+x/4u == bit));
    }
    CHECK(!trs80_render_video(pixels,100,512,192,TRS80_MODEL_I,0));
    trs80_write(0x3c00u,0xc1u);
    CHECK(trs80_render_video(pixels,512,512,192,TRS80_MODEL_I,0));
    CHECK(pixels[0] == 0xffffu && pixels[4] == 0u);
    CHECK(trs80_init(TRS80_MODEL_4) == TRS80_OK);
    trs80_out(0x84u,0x80u); trs80_write(0x3c00u,0x81u);
    CHECK(trs80_render_video(pixels,512,512,192,TRS80_MODEL_4,0));
    CHECK(pixels[0] == 0xffffu);
    trs80_out(0x84u,0u);
    CHECK(trs80_render_video(pixels,512,512,192,TRS80_MODEL_4,0));
    CHECK(pixels[0] == 0u);
    trs80_out(0x84u,0x0eu); trs80_write(0xf800u,0xa0u);
    CHECK(trs80_status().columns==80u && trs80_status().rows==24u);
    CHECK(trs80_render_video(pixels,512,512,288,TRS80_MODEL_4,0));
    CHECK(pixels[0] == 0xffffu);
    CHECK(trs80_init(TRS80_MODEL_I) == TRS80_OK);
    const uint8_t loop[] = {1,5,0,0x40,0xc3,0,0x40,2,2,0,0x40};
    CHECK(trs80_load_cmd(loop,sizeof(loop)) == TRS80_OK);
    trs80_set_audio_sink(sample,NULL);
    trs80_out(0xffu,1u);
    CHECK(trs80_run_cycles(177408u) == TRS80_OK);
    CHECK(samples>=2205u && samples<=2206u && nonzero==samples && last_sample>0);
    trs80_out(0xffu,2u);
    CHECK(trs80_run_cycles(17741u) == TRS80_OK);
    CHECK(last_sample<0);
    trs80_set_audio_sink(NULL,NULL); unsigned before=samples;
    CHECK(trs80_run_cycles(17741u) == TRS80_OK && samples==before);
    puts("video/audio: PASS"); return 0;
}
