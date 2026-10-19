/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program.h"
#include "cpu_profile.h"
#include <ctype.h>
#include <string.h>
static ti83_load_progress_t load_progress;
void ti83_program_progress_reset(void) {memset(&load_progress,0,sizeof(load_progress));}
ti83_load_progress_t ti83_program_progress(void) {return load_progress;}
#ifdef SDLPoP_PLATFORM_PICO
#include "compat.h"
#endif
static unsigned read_u16(const uint8_t *p) { return p[0] | ((unsigned)p[1]<<8); }
static int fail(char *error, size_t cap, const char *message)
{
    if (error && cap) snprintf(error,cap,"%s",message);
    return 0;
}
int ti83_program_extension(const char *path)
{
    size_t n=path?strlen(path):0;
    return n>=4 && path[n-4]=='.' && path[n-3]=='8' &&
        tolower((unsigned char)path[n-2])=='x' && tolower((unsigned char)path[n-1])=='p';
}
int ti83_program_parse(const uint8_t *b,size_t n,ti83_program_t *p,char *error,size_t cap)
{
    if (!b || !p || n<74 || memcmp(b,"**TI83F*\x1a\x0a\x00",11))
        return fail(error,cap,"NEED CLASSIC TI83+/84+ 8XP FILE");
    unsigned section=read_u16(b+53), header=read_u16(b+55);
    if (section+57u!=n || (header!=11 && header!=13) || section<header+4u)
        return fail(error,cap,"INVALID 8XP LENGTH");
    unsigned checksum=0;
    for (size_t i=55;i<n-2;++i) checksum+=b[i];
    if ((checksum&65535u)!=read_u16(b+n-2)) return fail(error,cap,"8XP CHECKSUM FAILED");
    unsigned length=read_u16(b+57), offset=59u+header;
    if (length<2 || length>24000 || offset+length!=n-2 || read_u16(b+57+header)!=length)
        return fail(error,cap,"NEED SINGLE RAM PROGRAM <=24K");
    if (b[59]!=5 && b[59]!=6) return fail(error,cap,"8XP IS NOT A PROGRAM");
    memset(p,0,sizeof(*p));
    for (unsigned i=0;i<8;++i) {
        unsigned ch=b[60+i];
        if (!ch) break;
        if ((ch<'A' || ch>'Z') && (i==0 || ch<'0' || ch>'9'))
            return fail(error,cap,"UNSUPPORTED PROGRAM NAME");
        p->name[i]=(char)ch;
    }
    if (!p->name[0]) return fail(error,cap,"EMPTY PROGRAM NAME");
    p->type=b[59]; p->version=header==13?b[68]:0;
    p->data=b+offset; p->size=(uint16_t)length;
    if (read_u16(p->data)!=length-2u) return fail(error,cap,"INVALID PROGRAM PAYLOAD");
    p->assembly=length>=4 && p->data[2]==0xbb && p->data[3]==0x6d;
    if (length>=4 && p->data[2]==0xef && (p->data[3]==0x7b || p->data[3]==0x69))
        return fail(error,cap,"CE/CSE GAMES NOT SUPPORTED");
    if (p->assembly && length>=5 && p->data[4]==0xc9)
        return fail(error,cap,"ION/MIRAGE SHELL GAME NOT SUPPORTED");
    return 1;
}
static void run_ms(TilemCalc *c,unsigned ms)
{
    while(ms) { unsigned part=ms>50?50:ms; ti83_step(c,part*1000); ms-=part; }
}
static int wait_ms(TilemCalc *c,unsigned ms,ti83_progress_t progress,void *context)
{
    while(ms) {
        if(progress && !progress(context))return 0;
        unsigned part=ms>10?10:ms;ti83_step(c,part*1000);ms-=part;
    }
    return 1;
}
static int key(TilemCalc *c,int code,ti83_progress_t progress,void *context)
{
    tilem_keypad_press_key(c,code);int ok=wait_ms(c,100,progress,context);
    tilem_keypad_release_key(c,code);return ok && wait_ms(c,150,progress,context);
}
int ti83_power_on_progress(TilemCalc *c,ti83_progress_t progress,void *context)
{
    if(!c)return 0;
    for(unsigned t=0;t<500;t+=10) {
        if(c->z80.halted && !c->lcd.active && !c->poweronhalt)break;
        if(!wait_ms(c,10,progress,context))return 0;
    }
    if(!key(c,TILEM_KEY_ON,progress,context))return 0;
    for(unsigned t=0;t<3000;t+=10) {
        if(c->z80.halted && c->lcd.active && c->poweronhalt)break;
        if(!wait_ms(c,10,progress,context))return 0;
    }
    return key(c,TILEM_KEY_ENTER,progress,context) && key(c,TILEM_KEY_CLEAR,progress,context);
}
void ti83_power_on(TilemCalc *c)
{
    (void)ti83_power_on_progress(c,NULL,NULL);
}
typedef struct { TilemCalc *calc; ti83_progress_t progress; void *context; unsigned elapsed, calls, cancelled; } Link;
static int advance(Link *l)
{
    if (++l->elapsed>120000) return 0;
    if (++l->calls%100==0 && l->progress && !l->progress(l->context)) {l->cancelled=1;return 0;}
    int remaining;
    (void)tilem_z80_run_time(l->calc,1000,&remaining);
    return 1;
}
static int send_byte(Link *l,uint8_t b)
{
    unsigned timeout=0;
    while (!tilem_linkport_graylink_ready(l->calc))
        if (++timeout>3000 || !advance(l)) return 0;
    return !tilem_linkport_graylink_send_byte(l->calc,b);
}
static int receive(Link *l)
{
    for (unsigned timeout=0;timeout<3000;++timeout) {
        int b=tilem_linkport_graylink_get_byte(l->calc);
        if (b>=0) return b;
        if (!advance(l)) break;
    }
    return -1;
}
static int packet(Link *l,uint8_t command,const uint8_t *data,unsigned size)
{
    if (!send_byte(l,0x23) || !send_byte(l,command) || !send_byte(l,(uint8_t)size) || !send_byte(l,(uint8_t)(size>>8))) return 0;
    unsigned sum=0;
    for(unsigned i=0;i<size;++i) {
        sum+=data[i];if(!send_byte(l,data[i]))return 0;
        if(command==0x15)load_progress.completed=i;
    }
    return !size || (send_byte(l,(uint8_t)sum) && send_byte(l,(uint8_t)(sum>>8)));
}
static int expect(Link *l,unsigned command)
{
    int id=receive(l),cmd=receive(l),lo=receive(l),hi=receive(l);
    return (id==0x73 || id==0x83) && cmd==(int)command && lo>=0 && hi>=0 && (command==0x09 || (lo==0 && hi==0));
}
static int remote(Link *l,unsigned code,int last)
{
    if (!send_byte(l,0x23) || !send_byte(l,0x87) || !send_byte(l,(uint8_t)code) || !send_byte(l,(uint8_t)(code>>8))) return 0;
    int ok=expect(l,0x56) && (last || expect(l,0x56));
    if(ok)++load_progress.completed;
    return ok;
}
int ti83_program_launch(TilemCalc *c,const ti83_program_t *p,ti83_progress_t progress,void *context,char *error,size_t cap)
{
    if (!c || !p || (c->hw.model_id!='4' && c->hw.model_id!='p')) return fail(error,cap,"GAME NEEDS TI83+/84+ SYSTEM");
    if (!p->data || p->size<2 || p->size>24000 || !p->name[0]) return fail(error,cap,"INVALID GAME DATA");
    Link l={c,progress,context,0,0,0};
    unsigned oldmask=c->z80.stop_mask;
    c->linkport.linkemu=TILEM_LINK_EMULATOR_GRAY;
    c->z80.stop_mask|=TILEM_STOP_LINK_READ_BYTE|TILEM_STOP_LINK_WRITE_BYTE|TILEM_STOP_LINK_ERROR;
    tilem_linkport_graylink_reset(c);
    uint8_t header[13]={p->size&255,p->size>>8,p->type};
    memcpy(header+3,p->name,8);header[11]=p->version;
    load_progress=(ti83_load_progress_t){1,0,p->size};
    ti83_profile_start();
#ifdef SDLPoP_PLATFORM_PICO
    uint64_t transfer_begin=compat_ticks_us();
#endif
    int ok=packet(&l,0xc9,header,13) && expect(&l,0x56) && expect(&l,0x09) &&
        packet(&l,0x56,NULL,0) && packet(&l,0x15,p->data,p->size) && expect(&l,0x56) && packet(&l,0x92,NULL,0);
#ifdef SDLPoP_PLATFORM_PICO
    uint64_t transfer_end=compat_ticks_us();
    unsigned transfer_calls=l.calls;
#endif
    if (!ok) fail(error,cap,"GAME TRANSFER FAILED / RAM OR OS");
    ti83_profile_report("transfer");
    ti83_profile_start();
    if (ok) {
        load_progress=(ti83_load_progress_t){2,0,5u+(p->assembly!=0)+(unsigned)strlen(p->name)};
        run_ms(c,200);
        ok=remote(&l,0x40,0) && remote(&l,0x09,0) && remote(&l,0x09,0);
        if (ok && p->assembly) ok=remote(&l,0xfc9c,0);
        if (ok) ok=remote(&l,0xda,0);
        for(unsigned i=0;ok && p->name[i];++i) {
            unsigned ch=(unsigned char)p->name[i];
            ok=remote(&l,ch>='A'?0x9a+ch-'A':0x8e + ch-'0',0);
        }
        if(ok) ok=remote(&l,0x05,1);
        if(!ok) fail(error,cap,"GAME IMPORTED; AUTO START FAILED");
    }
    c->z80.stop_mask=oldmask;
    tilem_linkport_graylink_reset(c);c->linkport.linkemu=TILEM_LINK_EMULATOR_NONE;
    ti83_profile_report("launch");
#ifdef SDLPoP_PLATFORM_PICO
    printf("TI83 IMPORT bytes=%u transfer_ms=%lu launch_ms=%lu transfer_calls=%u launch_calls=%u ok=%u\n",
        p->size,(unsigned long)((transfer_end-transfer_begin)/1000),
        (unsigned long)((compat_ticks_us()-transfer_end)/1000),transfer_calls,l.calls-transfer_calls,ok);
#endif
    /* Return immediately after Enter ACK; execution continues in game frames. */
    if(l.cancelled) fail(error,cap,"GAME LOAD CANCELLED");
    return ok;
}
