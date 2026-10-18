/* AquaOS - applications part 2: Calculator, Paint, Snake, Clock, System Info, Settings */
#include "aqua.h"

/* ====================================================================== */
/*  Calculator                                                             */
/* ====================================================================== */
static i32 c_acc, c_cur; static int c_op, c_fresh = 1, c_err, c_pressed = -1;
static const char *cbtn[20] = { "C", "+/-", "%", "/", "7", "8", "9", "*", "4", "5", "6", "-", "1", "2", "3", "+", "0", "0", "<-", "=" };

static void c_doop(void) {
    switch (c_op) {
    case '+': c_acc += c_cur; break;
    case '-': c_acc -= c_cur; break;
    case '*': c_acc *= c_cur; break;
    case '/': if (c_cur == 0) c_err = 1; else c_acc /= c_cur; break;
    default: c_acc = c_cur;
    }
}
static void calc_press(const char *b) {
    if (c_err && strcmp(b, "C")) return;
    if (b[0] >= '0' && b[0] <= '9' && !b[1]) {
        if (c_fresh) { c_cur = 0; c_fresh = 0; }
        if (c_cur < 100000000 && c_cur > -100000000) c_cur = c_cur * 10 + (c_cur < 0 ? -(b[0] - '0') : b[0] - '0');
    } else if (!strcmp(b, "C")) { c_acc = c_cur = 0; c_op = 0; c_fresh = 1; c_err = 0; }
    else if (!strcmp(b, "+/-")) c_cur = -c_cur;
    else if (!strcmp(b, "%")) c_cur /= 100;
    else if (!strcmp(b, "<-")) c_cur /= 10;
    else if (!strcmp(b, "=")) { if (c_op) { c_doop(); c_cur = c_acc; c_op = 0; } c_fresh = 1; }
    else {
        if (c_op && !c_fresh) { c_doop(); c_cur = c_acc; } else c_acc = c_cur;
        c_op = b[0]; c_fresh = 1;
    }
}
static void calc_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    fill_rect(cx, cy, cw, ch, RGB(36, 44, 60));
    fill_rrect(cx + 10, cy + 10, cw - 20, 54, 6, RGB(176, 224, 190));
    char b[24];
    if (c_err) scopy(b, "Error", 24); else ksnprintf(b, 24, "%d", c_cur);
    draw_text_s(cx + cw - 16 - (int)strlen(b) * 16, cy + 22, b, RGB(16, 36, 24), 2);
    if (c_op) { char o[2] = { (char)c_op, 0 }; draw_text(cx + 18, cy + 14, o, RGB(16, 36, 24)); }
    int bw = (cw - 20 - 12) / 4, bh = (ch - 76 - 10 - 16) / 5;
    for (int i = 0; i < 20; i++) {
        if (i == 17) continue;
        int r = i / 4, c = i % 4, w2 = (i == 16) ? 2 * bw + 4 : bw;
        int x = cx + 10 + c * (bw + 4), y = cy + 74 + r * (bh + 4);
        u32 col = (c == 3) ? RGB(240, 150, 50) : (r == 0 ? RGB(110, 124, 150) : RGB(70, 82, 106));
        if (i == c_pressed) col = mix(col, 0xFFFFFF, 90);
        fill_rrect(x, y, w2, bh, 6, col);
        draw_text(x + (w2 - text_w(cbtn[i])) / 2, y + (bh - 16) / 2, cbtn[i], RGB(255, 255, 255));
    }
}
static int calc_hit(int mx, int my, int cw, int ch) {
    int bw = (cw - 20 - 12) / 4, bh = (ch - 76 - 10 - 16) / 5;
    for (int i = 0; i < 20; i++) {
        if (i == 17) continue;
        int r = i / 4, c = i % 4, w2 = (i == 16) ? 2 * bw + 4 : bw;
        int x = 10 + c * (bw + 4), y = 74 + r * (bh + 4);
        if (mx >= x && mx < x + w2 && my >= y && my < y + bh) return i;
    }
    return -1;
}
static void calc_mouse(Window *w, int mx, int my, int btn, int ev) {
    (void)btn;
    int cx, cy, cw, ch; win_content(w, &cx, &cy, &cw, &ch);
    if (ev == EV_DOWN || ev == EV_DBL) { int h = calc_hit(mx, my, cw, ch); if (h >= 0) { c_pressed = h; calc_press(cbtn[h]); } }
    if (ev == EV_UP) c_pressed = -1;
}
static void calc_key(Window *w, int k) {
    (void)w;
    if (k >= '0' && k <= '9') { char b[2] = { (char)k, 0 }; calc_press(b); }
    else if (k == '+' || k == '-' || k == '*' || k == '/') { char b[2] = { (char)k, 0 }; calc_press(b); }
    else if (k == '\n' || k == '=') calc_press("=");
    else if (k == '\b') calc_press("<-");
    else if (k == 'c' || k == 'C' || k == 27) calc_press("C");
    else if (k == '%') calc_press("%");
}
const App app_calc = { "Calculator", "Calculator", 3, 260, 350, 0, calc_draw, calc_mouse, calc_key, 0, 0 };

/* ====================================================================== */
/*  Paint                                                                  */
/* ====================================================================== */
#define PW 392
#define PH 310
static u8 canvas[PW * PH];
static int p_col = 1, p_size = 3, p_lx = -1, p_ly, p_init;
static const u32 ppal[16] = {
    RGB(255, 255, 255), RGB(0, 0, 0), RGB(128, 128, 128), RGB(200, 200, 200), RGB(220, 40, 40), RGB(255, 140, 40),
    RGB(250, 220, 50), RGB(70, 190, 70), RGB(30, 130, 60), RGB(40, 200, 220), RGB(50, 110, 230), RGB(30, 40, 150),
    RGB(150, 70, 200), RGB(240, 110, 190), RGB(130, 80, 40), RGB(255, 200, 150)
};
static const int psizes[4] = { 1, 3, 6, 12 };
static void paint_init(Window *w) { (void)w; if (!p_init) { p_init = 1; memset(canvas, 0, sizeof canvas); } }
static void stamp(int x, int y, int col) {
    int s = psizes[p_size], h = s / 2;
    for (int j = 0; j < s; j++) for (int i = 0; i < s; i++) {
        int px = x - h + i, py = y - h + j;
        if (px >= 0 && px < PW && py >= 0 && py < PH) canvas[py * PW + px] = (u8)col;
    }
}
static void paint_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w; (void)cw;
    fill_rect(cx, cy, cw, ch, RGB(210, 218, 228));
    for (int i = 0; i < 16; i++) {
        int x = cx + 6 + (i & 1) * 32, y = cy + 6 + (i >> 1) * 28;
        fill_rect(x, y, 28, 24, ppal[i]);
        rect_outline(x, y, 28, 24, i == p_col ? RGB(255, 80, 0) : RGB(90, 100, 115));
        if (i == p_col) rect_outline(x + 1, y + 1, 26, 22, RGB(255, 255, 255));
    }
    for (int i = 0; i < 4; i++) {
        char b[4]; ksnprintf(b, 4, "%d", psizes[i]);
        ui_button(cx + 6 + (i & 1) * 32, cy + 234 + (i >> 1) * 26, 28, 22, b, i == p_size);
    }
    ui_button(cx + 6, cy + 288, 60, 22, "Clear", 0);
    int ox = cx + 74, oy = cy + 6;
    for (int y = 0; y < PH; y++) {
        int x = 0;
        while (x < PW) {
            int x2 = x + 1; u8 c = canvas[y * PW + x];
            while (x2 < PW && canvas[y * PW + x2] == c) x2++;
            fill_rect(ox + x, oy + y, x2 - x, 1, ppal[c]);
            x = x2;
        }
    }
    rect_outline(ox - 1, oy - 1, PW + 2, PH + 2, RGB(70, 80, 95));
}
static void paint_mouse(Window *w, int mx, int my, int btn, int ev) {
    (void)w;
    if (ev == EV_UP) { p_lx = -1; return; }
    if ((ev == EV_DOWN || ev == EV_DBL) && mx < 72) {
        for (int i = 0; i < 16; i++) {
            int x = 6 + (i & 1) * 32, y = 6 + (i >> 1) * 28;
            if (mx >= x && mx < x + 28 && my >= y && my < y + 24) p_col = i;
        }
        for (int i = 0; i < 4; i++) {
            int x = 6 + (i & 1) * 32, y = 234 + (i >> 1) * 26;
            if (mx >= x && mx < x + 28 && my >= y && my < y + 22) p_size = i;
        }
        if (mx >= 6 && mx < 66 && my >= 288 && my < 310) memset(canvas, 0, sizeof canvas);
        return;
    }
    int x = mx - 74, y = my - 6;
    int col = (btn & 2) ? 0 : p_col;
    if (ev == EV_DOWN || ev == EV_DBL) { p_lx = x; p_ly = y; stamp(x, y, col); }
    else if (ev == EV_MOVE && p_lx >= -1000) {
        if (p_lx == -1) { p_lx = x; p_ly = y; }
        int x0 = p_lx, y0 = p_ly, dx = abs(x - x0), dy = -abs(y - y0), sx = x0 < x ? 1 : -1, sy = y0 < y ? 1 : -1, e = dx + dy;
        for (;;) {
            stamp(x0, y0, col);
            if (x0 == x && y0 == y) break;
            int e2 = 2 * e;
            if (e2 >= dy) { e += dy; x0 += sx; }
            if (e2 <= dx) { e += dx; y0 += sy; }
        }
        p_lx = x; p_ly = y;
    }
}
const App app_paint = { "Paint", "Paint", 4, 480, 322, paint_init, paint_draw, paint_mouse, 0, 0, 0 };

/* ====================================================================== */
/*  Snake                                                                  */
/* ====================================================================== */
#define GW 24
#define GH 18
static int sx[GW * GH], sy[GW * GH], slen, sdir, sndir, fx, fy, salive, sstarted, sscore, sbest, sint;
static u32 slast;
static void snake_reset(void) {
    slen = 4; sdir = sndir = 1;
    for (int i = 0; i < slen; i++) { sx[i] = 8 - i; sy[i] = 9; }
    salive = 1; sstarted = 0; sscore = 0; sint = 120;
    ksrand(g_ms * 2654435761u + 77);
}
static void snake_food(void) {
    for (int t = 0; t < 500; t++) {
        fx = krand() % GW; fy = krand() % GH;
        int ok = 1; for (int i = 0; i < slen; i++) if (sx[i] == fx && sy[i] == fy) ok = 0;
        if (ok) return;
    }
}
static void snake_init(Window *w) { (void)w; snake_reset(); snake_food(); }
static void snake_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w; (void)cw; (void)ch;
    fill_rect(cx, cy, cw, ch, RGB(20, 30, 28));
    char b[64]; ksnprintf(b, sizeof b, "Score: %d    Best: %d", sscore, sbest);
    draw_text(cx + 8, cy + 6, b, RGB(150, 255, 170));
    int ox = cx, oy = cy + 28;
    for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
        fill_rect(ox + x * 16, oy + y * 16, 16, 16, ((x + y) & 1) ? RGB(30, 48, 42) : RGB(34, 54, 46));
    fill_rrect(ox + fx * 16 + 2, oy + fy * 16 + 2, 12, 12, 5, RGB(240, 70, 70));
    for (int i = slen - 1; i >= 0; i--) {
        u32 c = i == 0 ? RGB(150, 255, 150) : mix(RGB(70, 200, 100), RGB(30, 120, 70), i * 256 / (slen + 1));
        fill_rrect(ox + sx[i] * 16 + 1, oy + sy[i] * 16 + 1, 14, 14, 4, c);
    }
    if (slen) { fill_rect(ox + sx[0] * 16 + 4, oy + sy[0] * 16 + 4, 3, 3, 0); fill_rect(ox + sx[0] * 16 + 9, oy + sy[0] * 16 + 4, 3, 3, 0); }
    if (!salive || !sstarted) {
        fill_rect_alpha(ox, oy, GW * 16, GH * 16, 0, 140);
        const char *m = !salive ? "GAME OVER - press SPACE" : "Arrows / WASD to move - SPACE to start";
        draw_text(ox + (GW * 16 - text_w(m)) / 2, oy + GH * 8 - 8, m, RGB(255, 255, 255));
    }
}
static void snake_key(Window *w, int k) {
    (void)w;
    if (k == ' ') { if (!salive) { snake_reset(); snake_food(); } sstarted = !sstarted ? 1 : sstarted; return; }
    int d = -1;
    if (k == K_RIGHT || k == 'd') d = 1; else if (k == K_LEFT || k == 'a') d = 3;
    else if (k == K_UP || k == 'w') d = 0; else if (k == K_DOWN || k == 's') d = 2;
    if (d < 0) return;
    if (((d + 2) & 3) != sdir) sndir = d;
    if (salive) sstarted = 1;
}
static void snake_tick(Window *w) {
    if (!salive || !sstarted || g_ms - slast < (u32)sint) return;
    slast = g_ms; sdir = sndir;
    int nx = sx[0], ny = sy[0];
    if (sdir == 0) ny--; else if (sdir == 1) nx++; else if (sdir == 2) ny++; else nx--;
    if (nx < 0 || ny < 0 || nx >= GW || ny >= GH) { salive = 0; wm_inval_win(w); return; }
    for (int i = 0; i < slen - 1; i++) if (sx[i] == nx && sy[i] == ny) { salive = 0; wm_inval_win(w); return; }
    int grow = (nx == fx && ny == fy);
    if (grow) slen++;
    for (int i = slen - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
    sx[0] = nx; sy[0] = ny;
    if (grow) { sscore += 10; if (sscore > sbest) sbest = sscore; if (sint > 55) sint -= 4; snake_food(); speaker(1200, 15); }
    wm_inval_win(w);
}
const App app_snake = { "Snake", "Snake", 5, GW * 16, GH * 16 + 28, snake_init, snake_draw, 0, snake_key, snake_tick, 0 };

/* ====================================================================== */
/*  Clock & Calendar                                                       */
/* ====================================================================== */
static RTime ct; static u32 clast; static int csec = -1;
static const char *mnames[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
static const char *wdn[7] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
static int wday_of(int y, int m, int d) {
    static const int tt[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + tt[m - 1] + d) % 7;
}
static int dim(int y, int m) {
    static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0)) return 29;
    return d[m - 1];
}
static void thick(int x0, int y0, int x1, int y1, u32 c) { line(x0, y0, x1, y1, c); line(x0 + 1, y0, x1 + 1, y1, c); line(x0, y0 + 1, x1, y1 + 1, c); }
static void clock_init(Window *w) { (void)w; rtc_read(&ct); }
static void clock_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w; (void)cw; (void)ch;
    fill_rect(cx, cy, cw, ch, RGB(236, 242, 248));
    int mx = cx + 124, my = cy + 130;
    fill_circle(mx, my, 108, RGB(40, 70, 110));
    fill_circle(mx, my, 104, RGB(250, 252, 254));
    for (int i = 0; i < 60; i++) {
        int len = (i % 5) ? 4 : 10;
        int x0 = mx + isin(i * 6) * (100 - len) / 1000, y0 = my - icos(i * 6) * (100 - len) / 1000;
        int x1 = mx + isin(i * 6) * 100 / 1000, y1 = my - icos(i * 6) * 100 / 1000;
        line(x0, y0, x1, y1, (i % 5) ? RGB(150, 160, 175) : RGB(30, 50, 80));
    }
    int ha = (ct.hour % 12) * 30 + ct.min / 2, ma = ct.min * 6 + ct.sec / 10, sa = ct.sec * 6;
    thick(mx, my, mx + isin(ha) * 50 / 1000, my - icos(ha) * 50 / 1000, RGB(30, 50, 80));
    thick(mx, my, mx + isin(ma) * 76 / 1000, my - icos(ma) * 76 / 1000, RGB(46, 120, 170));
    line(mx, my, mx + isin(sa) * 88 / 1000, my - icos(sa) * 88 / 1000, RGB(220, 60, 60));
    fill_circle(mx, my, 5, RGB(30, 50, 80));
    int rx = cx + 250;
    char b[32];
    ksnprintf(b, sizeof b, "%02d:%02d:%02d", ct.hour, ct.min, ct.sec);
    draw_text_s(rx, cy + 14, b, RGB(30, 50, 80), 3);
    ksnprintf(b, sizeof b, "%s, %d %s %d", wdn[ct.wday % 7], ct.day, mnames[(ct.mon - 1) % 12], ct.year);
    draw_text(rx, cy + 66, b, RGB(60, 80, 105));
    int first = wday_of(ct.year, ct.mon, 1), nd = dim(ct.year, ct.mon);
    for (int i = 0; i < 7; i++) draw_text(rx + i * 24 + 4, cy + 96, wdn[i], RGB(46, 120, 170));
    for (int d = 1; d <= nd; d++) {
        int pos = first + d - 1, r = pos / 7, c = pos % 7;
        int x = rx + c * 24, y = cy + 116 + r * 22;
        if (d == ct.day) fill_rrect(x, y - 1, 22, 20, 5, g_acc_hi);
        char n[4]; ksnprintf(n, 4, "%d", d);
        draw_text(x + (22 - text_w(n)) / 2, y + 1, n, d == ct.day ? RGB(255, 255, 255) : RGB(30, 50, 80));
    }
}
static void clock_tick(Window *w) {
    if (g_ms - clast < 250) return;
    clast = g_ms; rtc_read(&ct);
    if (ct.sec != csec) { csec = ct.sec; wm_inval_win(w); }
}
const App app_clock = { "Clock", "Clock & Calendar", 6, 452, 272, clock_init, clock_draw, 0, 0, clock_tick, 0 };

/* ====================================================================== */
/*  System Info                                                            */
/* ====================================================================== */
static u32 si_last;
static void row(int cx, int *y, const char *k, const char *v) {
    draw_text(cx + 150, *y, k, RGB(70, 95, 130));
    draw_text(cx + 290, *y, v, RGB(20, 30, 45));
    *y += 20;
}
static void sys_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    gradient_v(cx, cy, cw, ch, RGB(246, 249, 252), RGB(222, 232, 242));
    /* logo */
    fill_circle(cx + 70, cy + 80, 52, g_acc_lo);
    fill_circle(cx + 70, cy + 80, 48, g_acc_hi);
    fill_circle(cx + 70, cy + 88, 30, RGB(240, 252, 255));
    fill_circle(cx + 56, cy + 74, 8, 0xFFFFFF);
    draw_text_s(cx + 18, cy + 150, "AquaOS", RGB(20, 70, 110), 2);
    char b[24]; ksnprintf(b, sizeof b, "version %s", AQUA_VERSION);
    draw_text(cx + 24, cy + 186, b, RGB(70, 95, 130));
    draw_text(cx + 24, cy + 206, ARCH_BITS == 64 ? "64-bit kernel" : "32-bit kernel", RGB(70, 95, 130));
    int y = cy + 14; char v[96]; RTime t; rtc_read(&t);
    row(cx, &y, "Operating system", "AquaOS (from scratch)");
    ksnprintf(v, sizeof v, "%s, %d-bit", ARCH_NAME, ARCH_BITS); row(cx, &y, "Kernel", v);
    ksnprintf(v, sizeof v, "%s", BI->cpu_lm ? "64-bit capable (long mode)" : "32-bit only"); row(cx, &y, "CPU mode", v);
    row(cx, &y, "CPU vendor", g_cpu.vendor);
    scopy(v, g_cpu.brand, 40); row(cx, &y, "CPU model", v);
    ksnprintf(v, sizeof v, "fam %u  model %u  step %u", g_cpu.family, g_cpu.model, g_cpu.stepping); row(cx, &y, "CPU id", v);
    u32 kb = mem_total_kb();
    ksnprintf(v, sizeof v, "%u MiB", kb / 1024); row(cx, &y, "Memory", v);
    ksnprintf(v, sizeof v, "%u KiB", ((u32)(uintptr_t)__bss_end - 0x100000) / 1024); row(cx, &y, "Kernel footprint", v);
    ksnprintf(v, sizeof v, "%dx%d, 32-bit colour", SW, SH); row(cx, &y, "Display", v);
    ksnprintf(v, sizeof v, "PS/2 keyboard%s", mouse_present ? " + mouse" : ""); row(cx, &y, "Input", v);
    ksnprintf(v, sizeof v, "%d files, %d bytes", fs_count(), fs_total_bytes()); row(cx, &y, "RAM disk", v);
    u32 s = g_ms / 1000; ksnprintf(v, sizeof v, "%um %02us", s / 60, s % 60); row(cx, &y, "Uptime", v);
    ksnprintf(v, sizeof v, "%02d.%02d.%04d %02d:%02d:%02d", t.day, t.mon, t.year, t.hour, t.min, t.sec); row(cx, &y, "Date / time", v);
    ksnprintf(v, sizeof v, "%s %s", __DATE__, __TIME__); row(cx, &y, "Built", v);
    /* ram bar */
    int by = cy + ch - 36;
    draw_text(cx + 150, by - 18, "RAM disk usage", RGB(70, 95, 130));
    fill_rrect(cx + 150, by, cw - 170, 14, 4, RGB(190, 202, 216));
    int cap = FS_MAX * FS_FILESZ, used = fs_total_bytes();
    int wbar = (cw - 170) * used / cap; if (used && wbar < 4) wbar = 4;
    if (wbar > 0) fill_rrect(cx + 150, by, wbar, 14, 4, g_acc_hi);
}
static void sys_tick(Window *w) { if (g_ms - si_last >= 1000) { si_last = g_ms; wm_inval_win(w); } }
const App app_sysinfo = { "System Info", "System Information", 7, 540, 350, 0, sys_draw, 0, 0, sys_tick, 0 };

/* ====================================================================== */
/*  Settings                                                               */
/* ====================================================================== */
static const u32 wtop[3] = { RGB(60, 190, 215), RGB(255, 170, 100), RGB(40, 55, 110) };
static const u32 wbot[3] = { RGB(4, 38, 92), RGB(100, 28, 92), RGB(4, 6, 20) };
static const char *wname[3] = { "Aqua Deep", "Coral Sunset", "Midnight" };
static const u32 acc[4] = { RGB(46, 170, 210), RGB(240, 120, 100), RGB(150, 110, 220), RGB(80, 190, 120) };
static void set_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w; (void)cw; (void)ch;
    fill_rect(cx, cy, cw, ch, RGB(244, 246, 249));
    draw_text(cx + 16, cy + 12, "Wallpaper", RGB(20, 60, 100));
    for (int i = 0; i < 3; i++) {
        int x = cx + 16 + i * 130, y = cy + 34;
        gradient_v(x, y, 120, 70, wtop[i], wbot[i]);
        rect_outline(x, y, 120, 70, i == g_theme ? RGB(255, 120, 0) : RGB(120, 130, 145));
        if (i == g_theme) rect_outline(x + 1, y + 1, 118, 68, 0xFFFFFF);
        draw_text(x + (120 - text_w(wname[i])) / 2, y + 76, wname[i], RGB(30, 45, 65));
    }
    draw_text(cx + 16, cy + 138, "Accent colour", RGB(20, 60, 100));
    for (int i = 0; i < 4; i++) {
        int x = cx + 36 + i * 60, y = cy + 176;
        fill_circle(x, y, 19, i == g_accent ? RGB(30, 40, 60) : RGB(200, 208, 218));
        fill_circle(x, y, 16, acc[i]);
    }
    int y = cy + 220;
    fill_rect(cx + 16, y, 18, 18, 0xFFFFFF); rect_outline(cx + 16, y, 18, 18, RGB(90, 100, 115));
    if (g_show_secs) { line(cx + 20, y + 9, cx + 24, y + 14, RGB(20, 130, 60)); line(cx + 24, y + 14, cx + 31, y + 4, RGB(20, 130, 60)); line(cx + 20, y + 10, cx + 24, y + 15, RGB(20, 130, 60)); line(cx + 24, y + 15, cx + 31, y + 5, RGB(20, 130, 60)); }
    draw_text(cx + 44, y + 1, "Show seconds in taskbar clock", RGB(30, 45, 65));
    ui_button(cx + 16, cy + 252, 130, 26, "Test speaker", 0);
    draw_text(cx + 16, cy + 292, "Settings are kept in RAM and reset on reboot.", RGB(110, 125, 145));
}
static void set_mouse(Window *w, int mx, int my, int btn, int ev) {
    (void)w; (void)btn;
    if (ev != EV_DOWN && ev != EV_DBL) return;
    for (int i = 0; i < 3; i++) { int x = 16 + i * 130; if (mx >= x && mx < x + 120 && my >= 34 && my < 104) { wm_set_theme(i, g_accent); return; } }
    for (int i = 0; i < 4; i++) { int x = 36 + i * 60; if (abs(mx - x) < 20 && abs(my - 176) < 20) { wm_set_theme(g_theme, i); return; } }
    if (mx >= 16 && mx < 300 && my >= 220 && my < 238) { g_show_secs = !g_show_secs; wm_inval_all(); }
    if (mx >= 16 && mx < 146 && my >= 252 && my < 278) speaker(660, 120);
}
const App app_settings = { "Settings", "Settings", 8, 400, 320, 0, set_draw, set_mouse, 0, 0, 0 };
