/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stddef.h>
#include <stdio.h>
#include "tilem.h"
#include <stdlib.h>
#include <string.h>
#ifdef SDLPoP_PLATFORM_PICO
#include "compat.h"
#endif
typedef union { size_t size; max_align_t alignment; } Header;
void *tilem_try_malloc(size_t size)
{
    if (size > 4u*1024u*1024u) return NULL;
#ifdef SDLPoP_PLATFORM_PICO
    Header *h = size >= 128u*1024u ? compat_large_malloc(sizeof(Header)+size) : compat_malloc(sizeof(Header)+size);
#else
    Header *h = malloc(sizeof(Header)+size);
#endif
    if (!h) return NULL;
    h->size = size;
    return h+1;
}
void *tilem_try_malloc0(size_t size) { void *p = tilem_try_malloc(size); if(p) memset(p,0,size); return p; }
void *tilem_try_malloc_atomic(size_t size) { return tilem_try_malloc(size); }
void *tilem_malloc(size_t size) { void *p = tilem_try_malloc(size); if(!p) abort(); return p; }
void *tilem_malloc0(size_t size) { void *p = tilem_malloc(size); memset(p,0,size); return p; }
void *tilem_malloc_atomic(size_t size) { return tilem_malloc(size); }
void tilem_free(void *p)
{
    if (!p) return;
#ifdef SDLPoP_PLATFORM_PICO
    compat_free((Header*)p-1);
#else
    free((Header*)p-1);
#endif
}
void *tilem_realloc(void *p, size_t size)
{
    void *q = tilem_malloc(size);
    if (p) { size_t old = ((Header*)p-1)->size; memcpy(q,p,old<size?old:size); tilem_free(p); }
    return q;
}
void tilem_message(TilemCalc *calc, const char *message, ...) { (void)calc; (void)message; }
void tilem_warning(TilemCalc *calc, const char *message, ...) { (void)calc; (void)message; }
void tilem_internal(TilemCalc *calc, const char *message, ...) { (void)calc; fprintf(stderr,"TI83 CORE INTERNAL: %s\n",message); }
