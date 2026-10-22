#include "gfx.h"

#include <math.h>
#include <string.h>

#include "font_data.h"

static uint32_t *fb;

void gfx_target(uint32_t *p) { fb = p; }

static inline uint32_t blend(uint32_t d, uint32_t c, int a)
{
    uint32_t rb = ((c & 0xff00ff) * a + (d & 0xff00ff) * (255 - a)) >> 8;
    uint32_t g = ((c & 0x00ff00) * a + (d & 0x00ff00) * (255 - a)) >> 8;
    return (rb & 0xff00ff) | (g & 0x00ff00);
}

uint32_t gfx_mix(uint32_t a, uint32_t b, float t)
{
    int k = (int)(t * 255.f);
    k = k < 0 ? 0 : k > 255 ? 255 : k;
    return blend(a, b, k);
}

void gfx_clear(uint32_t c)
{
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        fb[i] = c;
}

static int clip(int *x, int *y, int *w, int *h)
{
    if (*x < 0) {
        *w += *x;
        *x = 0;
    }
    if (*y < 0) {
        *h += *y;
        *y = 0;
    }
    if (*x + *w > SCREEN_W)
        *w = SCREEN_W - *x;
    if (*y + *h > SCREEN_H)
        *h = SCREEN_H - *y;
    return *w > 0 && *h > 0;
}

void gfx_fill(int x, int y, int w, int h, uint32_t c)
{
    if (!clip(&x, &y, &w, &h))
        return;
    for (int j = y; j < y + h; j++) {
        uint32_t *row = fb + j * SCREEN_W + x;
        for (int i = 0; i < w; i++)
            row[i] = c;
    }
}

void gfx_fill_a(int x, int y, int w, int h, uint32_t c, int alpha)
{
    if (!clip(&x, &y, &w, &h))
        return;
    for (int j = y; j < y + h; j++) {
        uint32_t *row = fb + j * SCREEN_W + x;
        for (int i = 0; i < w; i++)
            row[i] = blend(row[i], c, alpha);
    }
}

void gfx_rect(int x, int y, int w, int h, int t, uint32_t c)
{
    gfx_fill(x, y, w, t, c);
    gfx_fill(x, y + h - t, w, t, c);
    gfx_fill(x, y + t, t, h - 2 * t, c);
    gfx_fill(x + w - t, y + t, t, h - 2 * t, c);
}

/* Anti-aliased disc / ring via per-pixel distance coverage. */
static void shape(float cx, float cy, float r_out, float r_in, uint32_t c)
{
    int x0 = (int)floorf(cx - r_out - 1), x1 = (int)ceilf(cx + r_out + 1);
    int y0 = (int)floorf(cy - r_out - 1), y1 = (int)ceilf(cy + r_out + 1);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_W) x1 = SCREEN_W;
    if (y1 > SCREEN_H) y1 = SCREEN_H;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float dx = x + .5f - cx, dy = y + .5f - cy;
            float d = sqrtf(dx * dx + dy * dy);
            float a = r_out + .5f - d;
            if (r_in > 0.f) {
                float b = d - (r_in - .5f);
                if (b < a)
                    a = b;
            }
            if (a <= 0.f)
                continue;
            int ai = a >= 1.f ? 255 : (int)(a * 255.f);
            uint32_t *p = fb + y * SCREEN_W + x;
            *p = blend(*p, c, ai);
        }
}

void gfx_disc(float cx, float cy, float r, uint32_t c) { shape(cx, cy, r, 0.f, c); }
void gfx_ring(float cx, float cy, float r, float t, uint32_t c) { shape(cx, cy, r + t / 2, r - t / 2, c); }

int gfx_font_h(int font) { return font == FONT_LARGE ? FONT_LARGE_H : FONT_SMALL_H; }

int gfx_text_w(const char *s, int font)
{
    return (int)strlen(s) * (font == FONT_LARGE ? FONT_LARGE_W : FONT_SMALL_W);
}

int gfx_text(int x, int y, const char *s, uint32_t c, int font)
{
    int gw = font == FONT_LARGE ? FONT_LARGE_W : FONT_SMALL_W;
    int gh = font == FONT_LARGE ? FONT_LARGE_H : FONT_SMALL_H;
    for (; *s; s++, x += gw) {
        int ch = (unsigned char)*s;
        if (ch < 32 || ch > 126)
            ch = '?';
        const uint8_t *g = font == FONT_LARGE ? font_large_data[ch - 32] : font_small_data[ch - 32];
        for (int j = 0; j < gh; j++) {
            int py = y + j;
            if (py < 0 || py >= SCREEN_H)
                continue;
            for (int i = 0; i < gw; i++) {
                int px = x + i, a = g[j * gw + i];
                if (a && px >= 0 && px < SCREEN_W) {
                    uint32_t *p = fb + py * SCREEN_W + px;
                    *p = blend(*p, c, a);
                }
            }
        }
    }
    return x;
}
