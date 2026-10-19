/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cdp1802.h"
#include <string.h>
static uint8_t fetch(cdp1802_t *c, const cdp1802_bus_t *b) {
    uint16_t a = c->r[c->p]++;
    return b->read(b->context, a);
}
static void add(cdp1802_t *c, unsigned a, unsigned b, unsigned carry) {
    unsigned v = a + b + carry;
    c->d = (uint8_t)v;
    c->df = (uint8_t)(v >> 8);
}
static void sub(cdp1802_t *c, unsigned a, unsigned b, unsigned borrow) {
    int v = (int)a - (int)b - (int)borrow;
    c->d = (uint8_t)v;
    c->df = (uint8_t)(v >= 0);
}
void cdp1802_reset(cdp1802_t *c) {
    memset(c, 0, sizeof(*c));
    c->ie = 1;
}
uint8_t cdp1802_dma_out(cdp1802_t *c, const cdp1802_bus_t *b) {
    c->cycles++;
    return b->read(b->context, c->r[0]++);
}
void cdp1802_dma_in(cdp1802_t *c, const cdp1802_bus_t *b, uint8_t v) {
    c->cycles++;
    b->write(b->context, c->r[0]++, v);
}
void cdp1802_tick(cdp1802_t *c, const cdp1802_bus_t *b, uint8_t ef, int irq) {
    unsigned n;
    uint8_t v;
    int cond = 0;
    c->cycles++;
    if (c->phase == 0) {
        if (irq && c->ie) {
            c->t = (uint8_t)((c->x << 4) | c->p);
            c->x = 2;
            c->p = 1;
            c->ie = 0;
            c->idle = 0;
            return;
        }
        if (c->idle)
            return;
        c->op = fetch(c, b);
        c->phase = 1;
        return;
    }
    n = c->op & 15u;
    if (c->phase == 2) {
        if (n == 0 || n == 1 || n == 2 || n == 3 || n == 9 || n == 10 || n == 11) {
            v = fetch(c, b);
            cond = n == 0 || (n == 1 && c->q) || (n == 2 && c->d == 0) || (n == 3 && c->df) ||
                   (n == 9 && !c->q) || (n == 10 && c->d != 0) || (n == 11 && !c->df);
            if (cond)
                c->r[c->p] = (uint16_t)((c->high << 8) | v);
        } else {
            cond = (n == 5 && !c->q) || (n == 6 && c->d != 0) || (n == 7 && !c->df) || n == 8 ||
                   (n == 12 && c->ie) || (n == 13 && c->q) || (n == 14 && c->d == 0) ||
                   (n == 15 && c->df);
            if (cond)
                c->r[c->p] += 2;
        }
        c->phase = 0;
        c->instructions++;
        return;
    }
    c->phase = 0;
    c->instructions++;
    switch (c->op >> 4) {
    case 0:
        if (n == 0)
            c->idle = 1;
        else
            c->d = b->read(b->context, c->r[n]);
        break;
    case 1:
        c->r[n]++;
        break;
    case 2:
        c->r[n]--;
        break;
    case 3:
        if (n == 8) {
            c->r[c->p]++;
            break;
        }
        v = b->read(b->context, c->r[c->p]);
        switch (n & 7u) {
        case 0:
            cond = 1;
            break;
        case 1:
            cond = c->q;
            break;
        case 2:
            cond = c->d == 0;
            break;
        case 3:
            cond = c->df;
            break;
        default:
            cond = (ef >> ((n - 4u) & 3u)) & 1;
            break;
        }
        if (n & 8u)
            cond = !cond;
        if (cond)
            c->r[c->p] = (uint16_t)((c->r[c->p] & 0xff00u) | v);
        else
            c->r[c->p]++;
        break;
    case 4:
        c->d = b->read(b->context, c->r[n]++);
        break;
    case 5:
        b->write(b->context, c->r[n], c->d);
        break;
    case 6:
        if (n == 0)
            c->r[c->x]++;
        else if (n < 8) {
            v = b->read(b->context, c->r[c->x]++);
            if (b->output)
                b->output(b->context, n, v);
        } else if (n > 8) {
            v = b->input ? b->input(b->context, n - 8) : 0xff;
            c->d = v;
            b->write(b->context, c->r[c->x], v);
        }
        break;
    case 7:
        switch (n) {
        case 0:
        case 1:
            v = b->read(b->context, c->r[c->x]++);
            c->x = v >> 4;
            c->p = v & 15;
            c->ie = n == 0;
            break;
        case 2:
            c->d = b->read(b->context, c->r[c->x]++);
            break;
        case 3:
            b->write(b->context, c->r[c->x]--, c->d);
            break;
        case 4:
            add(c, c->d, b->read(b->context, c->r[c->x]), c->df);
            break;
        case 5:
            sub(c, b->read(b->context, c->r[c->x]), c->d, !c->df);
            break;
        case 6:
            v = c->d & 1;
            c->d = (uint8_t)((c->d >> 1) | (c->df << 7));
            c->df = v;
            break;
        case 7:
            sub(c, c->d, b->read(b->context, c->r[c->x]), !c->df);
            break;
        case 8:
            b->write(b->context, c->r[c->x], c->t);
            break;
        case 9:
            c->t = (uint8_t)((c->x << 4) | c->p);
            b->write(b->context, c->r[2]--, c->t);
            c->x = c->p;
            break;
        case 10:
            c->q = 0;
            break;
        case 11:
            c->q = 1;
            break;
        case 12:
            add(c, c->d, fetch(c, b), c->df);
            break;
        case 13:
            sub(c, fetch(c, b), c->d, !c->df);
            break;
        case 14:
            v = c->d >> 7;
            c->d = (uint8_t)((c->d << 1) | c->df);
            c->df = v;
            break;
        case 15:
            sub(c, c->d, fetch(c, b), !c->df);
            break;
        }
        break;
    case 8:
        c->d = (uint8_t)c->r[n];
        break;
    case 9:
        c->d = (uint8_t)(c->r[n] >> 8);
        break;
    case 10:
        c->r[n] = (uint16_t)((c->r[n] & 0xff00u) | c->d);
        break;
    case 11:
        c->r[n] = (uint16_t)((c->r[n] & 255u) | (c->d << 8));
        break;
    case 12:
        if (n == 0 || n == 1 || n == 2 || n == 3 || n == 9 || n == 10 || n == 11)
            c->high = fetch(c, b);
        c->phase = 2;
        c->instructions--;
        break;
    case 13:
        c->p = (uint8_t)n;
        break;
    case 14:
        c->x = (uint8_t)n;
        break;
    case 15:
        v = (n == 6 || n == 14) ? 0 : ((n & 8u) ? fetch(c, b) : b->read(b->context, c->r[c->x]));
        switch (n & 7u) {
        case 0:
            c->d = v;
            break;
        case 1:
            c->d |= v;
            break;
        case 2:
            c->d &= v;
            break;
        case 3:
            c->d ^= v;
            break;
        case 4:
            add(c, c->d, v, 0);
            break;
        case 5:
            sub(c, v, c->d, 0);
            break;
        case 6:
            if (n & 8u) {
                c->df = c->d >> 7;
                c->d <<= 1;
            } else {
                c->df = c->d & 1;
                c->d >>= 1;
            }
            break;
        case 7:
            sub(c, c->d, v, 0);
            break;
        }
        break;
    }
}
