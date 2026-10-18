/* AquaOS - software graphics: linear framebuffer, double buffer, clipping, text */
#include "aqua.h"

int SW, SH;
u32 *g_fb;
int g_fb_pitch;
u32 g_back[MAXW * MAXH];
u32 g_wall[MAXW * MAXH];

static u8 font[4096];
static u32 *dst;
static int dpitch;
static int cx0, cy0, cx1, cy1;      /* clip, exclusive max */

void gfx_init(void) {
    SW = BI->fb_w; SH = BI->fb_h;
    if (SW > MAXW) SW = MAXW;
    if (SH > MAXH) SH = MAXH;
    g_fb = (u32 *)(uintptr_t)BI->fb_addr;
    g_fb_pitch = BI->fb_pitch / 4;
    memcpy(font, (void *)(uintptr_t)BI->font_addr, 4096);
    gfx_target(g_back, SW);
}
void gfx_target(u32 *buf, int pitch) { dst = buf; dpitch = pitch; gfx_clipreset(); }
void gfx_clipreset(void) { cx0 = 0; cy0 = 0; cx1 = SW; cy1 = SH; }
Rect gfx_getclip(void) { Rect r = { cx0, cy0, cx1 - cx0, cy1 - cy0 }; return r; }
void gfx_setclip_r(Rect r) { gfx_setclip(r.x, r.y, r.w, r.h); }
void gfx_setclip(int x, int y, int w, int h) {
    cx0 = x < 0 ? 0 : x; cy0 = y < 0 ? 0 : y;
    cx1 = x + w > SW ? SW : x + w; cy1 = y + h > SH ? SH : y + h;
    if (cx1 < cx0) cx1 = cx0;
    if (cy1 < cy0) cy1 = cy0;
}
Rect rect_isect(Rect a, Rect b) {
    int x0 = a.x > b.x ? a.x : b.x, y0 = a.y > b.y ? a.y : b.y;
    int x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    Rect r = { x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0 };
    return r;
}
Rect rect_union(Rect a, Rect b) {
    if (a.w <= 0 || a.h <= 0) return b;
    if (b.w <= 0 || b.h <= 0) return a;
    int x0 = a.x < b.x ? a.x : b.x, y0 = a.y < b.y ? a.y : b.y;
    int x1 = a.x + a.w > b.x + b.w ? a.x + a.w : b.x + b.w;
    int y1 = a.y + a.h > b.y + b.h ? a.y + a.h : b.y + b.h;
    Rect r = { x0, y0, x1 - x0, y1 - y0 };
    return r;
}
int rect_hit(Rect r, int x, int y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }

u32 mix(u32 a, u32 b, int t) {
    int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255;
    int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255;
    return RGB((ar * (256 - t) + br * t) >> 8, (ag * (256 - t) + bg * t) >> 8, (ab * (256 - t) + bb * t) >> 8);
}

static inline void px(int x, int y, u32 c) {
    if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) dst[y * dpitch + x] = c;
}

void fill_rect(int x, int y, int w, int h, u32 c) {
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; yy++) memset32(dst + yy * dpitch + x0, c, x1 - x0);
}
void fill_rect_alpha(int x, int y, int w, int h, u32 c, int a) {
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    if (x1 <= x0 || y1 <= y0) return;
    int sr = ((c >> 16) & 255) * a, sg = ((c >> 8) & 255) * a, sb = (c & 255) * a, ia = 256 - a;
    for (int yy = y0; yy < y1; yy++) {
        u32 *p = dst + yy * dpitch + x0;
        for (int xx = x0; xx < x1; xx++, p++) {
            u32 d = *p;
            *p = RGB((((d >> 16) & 255) * ia + sr) >> 8, (((d >> 8) & 255) * ia + sg) >> 8, ((d & 255) * ia + sb) >> 8);
        }
    }
}
static int isqrt(int v) { int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
void fill_rrect(int x, int y, int w, int h, int r, u32 c) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r <= 0) { fill_rect(x, y, w, h, c); return; }
    for (int i = 0; i < h; i++) {
        int inset = 0, k = i < r ? i : (i >= h - r ? h - 1 - i : r);
        if (k < r) { int dy = r - k; inset = r - isqrt(r * r - dy * dy); }
        fill_rect(x + inset, y + i, w - 2 * inset, 1, c);
    }
}
void hline(int x, int y, int w, u32 c) { fill_rect(x, y, w, 1, c); }
void vline(int x, int y, int h, u32 c) { fill_rect(x, y, 1, h, c); }
void rect_outline(int x, int y, int w, int h, u32 c) {
    hline(x, y, w, c); hline(x, y + h - 1, w, c); vline(x, y, h, c); vline(x + w - 1, y, h, c);
}
void line(int x0, int y0, int x1, int y1, u32 c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void fill_circle(int cx, int cy, int r, u32 c) {
    for (int dy = -r; dy <= r; dy++) { int dx = isqrt(r * r - dy * dy); hline(cx - dx, cy + dy, 2 * dx + 1, c); }
}
void circle(int cx, int cy, int r, u32 c) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        px(cx + x, cy + y, c); px(cx - x, cy + y, c); px(cx + x, cy - y, c); px(cx - x, cy - y, c);
        px(cx + y, cy + x, c); px(cx - y, cy + x, c); px(cx + y, cy - x, c); px(cx - y, cy - x, c);
        y++;
        if (err < 0) err += 2 * y + 1; else { x--; err += 2 * (y - x) + 1; }
    }
}
void gradient_v(int x, int y, int w, int h, u32 c1, u32 c2) {
    for (int i = 0; i < h; i++) fill_rect(x, y + i, w, 1, mix(c1, c2, h > 1 ? i * 256 / (h - 1) : 0));
}

/* ---- text (8x16 VGA ROM font) ---- */
void draw_char(int x, int y, int ch, u32 fg) {
    if (x + 8 <= cx0 || x >= cx1 || y + 16 <= cy0 || y >= cy1) return;
    const u8 *g = font + ((u8)ch) * 16;
    for (int r = 0; r < 16; r++) {
        int py = y + r;
        if (py < cy0 || py >= cy1) continue;
        int bits = g[r];
        if (!bits) continue;
        u32 *row = dst + py * dpitch;
        for (int c = 0; c < 8; c++)
            if (bits & (0x80 >> c)) { int pxx = x + c; if (pxx >= cx0 && pxx < cx1) row[pxx] = fg; }
    }
}
void draw_text(int x, int y, const char *s, u32 fg) {
    for (; *s; s++, x += 8) draw_char(x, y, *s, fg);
}
void draw_text_s(int x, int y, const char *s, u32 fg, int sc) {
    if (sc <= 1) { draw_text(x, y, s, fg); return; }
    for (; *s; s++, x += 8 * sc) {
        const u8 *g = font + ((u8)*s) * 16;
        for (int r = 0; r < 16; r++)
            for (int c = 0; c < 8; c++)
                if (g[r] & (0x80 >> c)) fill_rect(x + c * sc, y + r * sc, sc, sc, fg);
    }
}
int text_w(const char *s) { return (int)strlen(s) * 8; }

/* ---- presenting ---- */
void gfx_blit(int x, int y, int w, int h) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > SW ? SW : x + w, y1 = y + h > SH ? SH : y + h;
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; yy++) memcpy32(g_fb + yy * g_fb_pitch + x0, g_back + yy * SW + x0, x1 - x0);
}
void gfx_blit_all(void) { gfx_blit(0, 0, SW, SH); }

/* ---- mouse cursor (drawn straight onto the framebuffer) ---- */
static const char *cur_img[19] = {
    "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ", "X....X      ",
    "X.....X     ", "X......X    ", "X.......X   ", "X........X  ", "X.........X ", "X......XXXXX",
    "X...X..X    ", "X..X X..X   ", "X.X  X..X   ", "XX    X..X  ", "X      X..X ", "       X..X ",
    "        XX  "
};
void cursor_draw(int x, int y) {
    for (int r = 0; r < 19; r++) {
        if (y + r < 0 || y + r >= SH) continue;
        for (int c = 0; c < 12; c++) {
            char ch = cur_img[r][c];
            if (ch == ' ' || x + c < 0 || x + c >= SW) continue;
            g_fb[(y + r) * g_fb_pitch + x + c] = ch == 'X' ? 0x000000 : 0xFFFFFF;
        }
    }
}
void cursor_erase(int x, int y) { gfx_blit(x, y, 12, 19); }

/* ---- wallpaper ---- */
void wall_render(int theme) {
    static const u32 top[3] = { RGB(60, 190, 215), RGB(255, 170, 100), RGB(40, 55, 110) };
    static const u32 bot[3] = { RGB(4, 38, 92),    RGB(100, 28, 92),   RGB(4, 6, 20) };
    u32 *old = dst; int op = dpitch; Rect oc = gfx_getclip();
    gfx_target(g_wall, SW);
    for (int y = 0; y < SH; y++) fill_rect(0, y, SW, 1, mix(top[theme], bot[theme], y * 256 / SH));
    /* bubbles */
    u32 s = 0xC0FFEE11;
    for (int k = 0; k < 30; k++) {
        s = s * 1664525u + 1013904223u; int bx = (s >> 8) % SW;
        s = s * 1664525u + 1013904223u; int by = (s >> 8) % SH;
        s = s * 1664525u + 1013904223u; int r = 5 + (s >> 8) % 26;
        u32 base = mix(top[theme], bot[theme], by * 256 / SH);
        circle(bx, by, r, mix(base, 0xFFFFFF, 90));
        circle(bx, by, r - 1, mix(base, 0xFFFFFF, 40));
        fill_circle(bx - r / 3, by - r / 3, r / 5 + 1, mix(base, 0xFFFFFF, 130));
    }
    /* layered waves */
    for (int L = 0; L < 3; L++) {
        int baseY = SH * (56 + 13 * L) / 100, amp = 16 + 7 * L, period = 340 - 60 * L, phase = L * 80;
        u32 col = L == 0 ? 0xFFFFFF : RGB(0, 10, 40);
        int alpha = L == 0 ? 22 : 38;
        for (int x = 0; x < SW; x++) {
            int y = baseY + isin(x * 360 / period + phase) * amp / 1000;
            fill_rect_alpha(x, y, 1, SH - y, col, alpha);
        }
    }
    dst = old; dpitch = op; gfx_setclip_r(oc);
}
