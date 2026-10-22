/* Minimal software renderer into a 640x480 XRGB8888 buffer. */
#pragma once
#include <stdint.h>

#define SCREEN_W 640
#define SCREEN_H 480

enum { FONT_SMALL, FONT_LARGE };

void gfx_target(uint32_t *fb);
void gfx_clear(uint32_t c);
void gfx_fill(int x, int y, int w, int h, uint32_t c);
void gfx_fill_a(int x, int y, int w, int h, uint32_t c, int alpha);
void gfx_rect(int x, int y, int w, int h, int t, uint32_t c);
void gfx_disc(float cx, float cy, float r, uint32_t c);
void gfx_ring(float cx, float cy, float r, float t, uint32_t c);
int gfx_text(int x, int y, const char *s, uint32_t c, int font);
int gfx_text_w(const char *s, int font);
int gfx_font_h(int font);
uint32_t gfx_mix(uint32_t a, uint32_t b, float t);
