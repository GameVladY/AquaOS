/* AquaOS - window manager / compositor: desktop, windows, taskbar, start menu */
#include "aqua.h"

#define MAXAPPS 16
int g_theme, g_accent, g_show_secs = 0;
u32 g_acc_hi = RGB(46, 170, 210), g_acc_lo = RGB(20, 110, 160);

static Window wins[MAXAPPS];
static int zord[MAXAPPS], nz;
static int start_open, menu_hover = -1, sel_icon = -1;
static int drag_win = -1, drag_dx, drag_dy, cap_win = -1;
static Rect dirty; static int dirty_v;
static RTime now; static u32 last_clock;
static u32 dbl_ms; static int dbl_id = -99, dbl_x, dbl_y;

#define ICON_W 88
#define ICON_H 86
#define MENU_W 270
#define MENU_ITEM 30
#define MENU_HEAD 38

/* ------------------------------------------------------------------ */
static const u32 acc_hi[4] = { RGB(46, 170, 210), RGB(240, 120, 100), RGB(150, 110, 220), RGB(80, 190, 120) };
static const u32 acc_lo[4] = { RGB(20, 110, 160), RGB(190, 70, 70),   RGB(100, 70, 170),  RGB(40, 130, 80) };

void wm_set_theme(int theme, int accent) {
    g_theme = theme; g_accent = accent;
    g_acc_hi = acc_hi[accent]; g_acc_lo = acc_lo[accent];
    wall_render(theme);
    wm_inval_all();
}

void wm_inval(int x, int y, int w, int h) {
    Rect r = { x, y, w, h }, s = { 0, 0, SW, SH };
    r = rect_isect(r, s);
    if (r.w <= 0 || r.h <= 0) return;
    dirty = dirty_v ? rect_union(dirty, r) : r;
    dirty_v = 1;
}
void wm_inval_all(void) { dirty_v = 0; wm_inval(0, 0, SW, SH); }
void wm_inval_win(Window *w) { wm_inval(w->x, w->y, w->w + 8, w->h + 8); }

void wm_init(void) {
    memset(wins, 0, sizeof wins);
    nz = 0;
    for (int i = 0; i < g_napps; i++) wins[i].app = g_apps[i];
    wm_set_theme(g_theme, g_accent);
    rtc_read(&now);
}
Window *wm_win(int idx) { return &wins[idx]; }

void win_content(Window *w, int *x, int *y, int *cw, int *ch) {
    *x = w->x + 1; *y = w->y + TITLE_H; *cw = w->w - 2; *ch = w->h - TITLE_H - 1;
}

static int widx(Window *w) { return (int)(w - wins); }
static void z_remove(int idx) {
    for (int i = 0; i < nz; i++) if (zord[i] == idx) { for (int j = i; j < nz - 1; j++) zord[j] = zord[j + 1]; nz--; return; }
}
static void z_raise(int idx) { z_remove(idx); zord[nz++] = idx; }
static int focus_idx(void) {
    for (int i = nz - 1; i >= 0; i--) if (wins[zord[i]].open && !wins[zord[i]].min) return zord[i];
    return -1;
}

Window *wm_open(int idx) {
    Window *w = &wins[idx];
    if (!w->open) {
        int cw = w->app->w, ch = w->app->h;
        w->w = cw + 2; w->h = ch + TITLE_H + 1;
        if (w->w > SW - 16) w->w = SW - 16;
        if (w->h > SH - TASK_H - 8) w->h = SH - TASK_H - 8;
        w->x = 110 + (idx % 5) * 30; w->y = 30 + (idx % 5) * 28;
        if (w->x + w->w > SW) w->x = SW - w->w;
        if (w->y + w->h > SH - TASK_H) w->y = SH - TASK_H - w->h;
        if (w->x < 0) w->x = 0;
        if (w->y < 0) w->y = 0;
        w->open = 1; w->max = 0;
        if (w->app->init) w->app->init(w);
    }
    w->min = 0;
    z_raise(idx);
    wm_inval_win(w);
    return w;
}
void wm_close(Window *w) {
    wm_inval_win(w);
    w->open = 0; w->min = 0;
    z_remove(widx(w));
    if (w->app->closed) w->app->closed(w);
    wm_inval(0, SH - TASK_H, SW, TASK_H);
}
static void win_toggle_max(Window *w) {
    wm_inval_win(w);
    if (!w->max) {
        w->sx = w->x; w->sy = w->y; w->sw = w->w; w->sh = w->h;
        w->x = 0; w->y = 0; w->w = SW; w->h = SH - TASK_H; w->max = 1;
    } else { w->x = w->sx; w->y = w->sy; w->w = w->sw; w->h = w->sh; w->max = 0; }
    wm_inval_win(w);
}

/* ------------------------------------------------------------------ icons */
static int sc(int v, int s) { int r = v * s / 32; return (r < 1 && v > 0) ? 1 : r; }
void draw_icon(int id, int x, int y, int s) {
    #define S(v) sc(v, s)
    int cx = x + S(16), cy = y + S(16);
    switch (id) {
    case 0: /* terminal */
        fill_rrect(x + S(2), y + S(4), S(28), S(24), S(3), RGB(26, 30, 42));
        fill_rrect(x + S(2), y + S(4), S(28), S(6), S(3), RGB(90, 100, 125));
        line(x + S(7), y + S(14), x + S(12), y + S(18), RGB(90, 255, 150));
        line(x + S(12), y + S(18), x + S(7), y + S(22), RGB(90, 255, 150));
        if (s >= 32) { line(x + S(7), y + S(15), x + S(12), y + S(19), RGB(90, 255, 150)); line(x + S(12), y + S(19), x + S(7), y + S(23), RGB(90, 255, 150)); }
        fill_rect(x + S(15), y + S(22), S(9), S(2), RGB(90, 255, 150));
        break;
    case 1: /* notepad */
        fill_rrect(x + S(6), y + S(2), S(20), S(28), S(2), RGB(250, 250, 250));
        rect_outline(x + S(6), y + S(2), S(20), S(28), RGB(150, 160, 175));
        fill_rect(x + S(6), y + S(2), S(20), S(6), RGB(60, 130, 210));
        for (int i = 0; i < 4; i++) fill_rect(x + S(9), y + S(12) + i * S(4), S(14), S(1), RGB(150, 160, 175));
        break;
    case 2: /* files */
        fill_rrect(x + S(3), y + S(6), S(13), S(7), S(2), RGB(226, 160, 30));
        fill_rrect(x + S(3), y + S(10), S(26), S(18), S(3), RGB(246, 196, 64));
        fill_rect(x + S(4), y + S(13), S(24), S(1), RGB(255, 225, 130));
        break;
    case 3: /* calculator */
        fill_rrect(x + S(5), y + S(2), S(22), S(28), S(3), RGB(58, 68, 86));
        fill_rect(x + S(8), y + S(5), S(16), S(7), RGB(150, 230, 170));
        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++)
            fill_rect(x + S(8) + c * S(6), y + S(15) + r * S(5), S(4), S(3), c == 2 ? RGB(250, 160, 60) : RGB(200, 210, 225));
        break;
    case 4: /* paint */
        fill_rrect(x + S(2), y + S(5), S(28), S(22), S(9), RGB(205, 160, 110));
        fill_circle(x + S(9), y + S(12), S(3), RGB(230, 60, 60));
        fill_circle(x + S(16), y + S(9), S(3), RGB(60, 110, 230));
        fill_circle(x + S(23), y + S(12), S(3), RGB(60, 190, 90));
        fill_circle(x + S(22), y + S(19), S(3), RGB(245, 215, 60));
        fill_circle(x + S(12), y + S(20), S(3), RGB(120, 90, 60));
        break;
    case 5: /* snake */
        fill_rect(x + S(4), y + S(4), S(24), S(24), RGB(30, 60, 40));
        for (int i = 0; i < 4; i++) fill_rect(x + S(6) + i * S(5), y + S(21), S(5), S(5), RGB(90, 220, 110));
        fill_rect(x + S(21), y + S(16), S(5), S(5), RGB(90, 220, 110));
        fill_rect(x + S(21), y + S(11), S(5), S(5), RGB(140, 255, 150));
        fill_circle(x + S(9), y + S(10), S(2), RGB(240, 70, 70));
        break;
    case 6: /* clock */
        fill_circle(cx, cy, S(14), RGB(245, 248, 250));
        circle(cx, cy, S(14), RGB(60, 80, 110));
        line(cx, cy, cx, cy - S(9), RGB(30, 40, 60));
        line(cx, cy, cx + S(6), cy + S(2), RGB(200, 60, 60));
        fill_circle(cx, cy, S(2), RGB(30, 40, 60));
        break;
    case 7: /* system info monitor */
        fill_rrect(x + S(3), y + S(4), S(26), S(18), S(3), RGB(50, 60, 80));
        fill_rect(x + S(5), y + S(6), S(22), S(14), RGB(60, 190, 230));
        fill_rect(x + S(8), y + S(14), S(3), S(4), RGB(255, 255, 255));
        fill_rect(x + S(13), y + S(11), S(3), S(7), RGB(255, 255, 255));
        fill_rect(x + S(18), y + S(9), S(3), S(9), RGB(255, 255, 255));
        fill_rect(x + S(13), y + S(22), S(6), S(4), RGB(130, 140, 155));
        fill_rrect(x + S(9), y + S(26), S(14), S(3), S(1), RGB(130, 140, 155));
        break;
    case 8: /* settings gear */
        for (int k = 0; k < 8; k++) {
            int tx = cx + icos(k * 45) * S(11) / 1000, ty = cy + isin(k * 45) * S(11) / 1000;
            fill_rect(tx - S(2), ty - S(2), S(4), S(4), RGB(150, 160, 175));
        }
        fill_circle(cx, cy, S(9), RGB(150, 160, 175));
        fill_circle(cx, cy, S(4), RGB(70, 80, 100));
        break;
    }
    #undef S
}

void ui_button(int x, int y, int w, int h, const char *label, int pressed) {
    fill_rrect(x, y, w, h, 4, RGB(150, 162, 178));
    fill_rrect(x + 1, y + 1, w - 2, h - 2, 3, pressed ? RGB(188, 204, 218) : RGB(228, 235, 242));
    int tw = text_w(label);
    draw_text(x + (w - tw) / 2, y + (h - 16) / 2, label, RGB(25, 35, 50));
}

/* ------------------------------------------------------------------ geometry helpers */
static int icons_per_col(void) { int n = (SH - TASK_H - 14) / ICON_H; return n < 1 ? 1 : n; }
static Rect icon_rect(int i) {
    int per = icons_per_col();
    Rect r = { 8 + (i / per) * ICON_W, 10 + (i % per) * ICON_H, ICON_W - 4, ICON_H - 4 };
    return r;
}
static Rect start_btn(void) { Rect r = { 6, SH - TASK_H + 4, 104, TASK_H - 8 }; return r; }
static Rect menu_rect(void) {
    int h = MENU_HEAD + g_napps * MENU_ITEM + 12 + 2 * MENU_ITEM + 8;
    Rect r = { 6, SH - TASK_H - h - 2, MENU_W, h };
    return r;
}
static int menu_item_at(int mx, int my) {  /* 0..napps-1 apps, napps=restart, napps+1=shutdown, -1 none */
    Rect m = menu_rect();
    if (!rect_hit(m, mx, my)) return -1;
    int y = my - m.y - MENU_HEAD;
    if (y < 0) return -1;
    if (y < g_napps * MENU_ITEM) return y / MENU_ITEM;
    y -= g_napps * MENU_ITEM + 12;
    if (y < 0) return -1;
    int k = y / MENU_ITEM;
    return k < 2 ? g_napps + k : -1;
}
static int task_count(void) { int n = 0; for (int i = 0; i < g_napps; i++) n += wins[i].open; return n; }
static Rect task_rect(int slot, int n) {
    int x0 = 118, avail = SW - x0 - 180;
    int w = n ? avail / n - 4 : 150;
    if (w > 160) w = 160;
    Rect r = { x0 + slot * (w + 4), SH - TASK_H + 4, w, TASK_H - 8 };
    return r;
}
static Rect tray_rect(void) { Rect r = { SW - 172, SH - TASK_H, 172, TASK_H }; return r; }

/* ------------------------------------------------------------------ drawing */
static void draw_window(Window *w, int focused) {
    fill_rect_alpha(w->x + 6, w->y + 6, w->w, w->h, 0x000000, 60);
    fill_rect(w->x, w->y, w->w, w->h, focused ? g_acc_lo : RGB(90, 98, 110));
    u32 hi = focused ? g_acc_hi : RGB(135, 145, 158), lo = focused ? g_acc_lo : RGB(98, 108, 120);
    gradient_v(w->x + 1, w->y + 1, w->w - 2, TITLE_H - 1, hi, lo);
    draw_icon(w->app->icon, w->x + 6, w->y + 4, 18);
    draw_text(w->x + 30 + 1, w->y + 5 + 1, w->app->title, RGB(0, 40, 70));
    draw_text(w->x + 30, w->y + 5, w->app->title, RGB(255, 255, 255));
    int bx = w->x + w->w - 26;
    fill_rrect(bx, w->y + 4, 22, 18, 4, RGB(215, 70, 70));
    line(bx + 7, w->y + 9, bx + 14, w->y + 16, 0xFFFFFF); line(bx + 14, w->y + 9, bx + 7, w->y + 16, 0xFFFFFF);
    bx -= 25;
    fill_rrect(bx, w->y + 4, 22, 18, 4, mix(hi, 0xFFFFFF, 60));
    rect_outline(bx + 6, w->y + 8, 10, 9, RGB(20, 50, 80)); hline(bx + 6, w->y + 9, 10, RGB(20, 50, 80));
    bx -= 25;
    fill_rrect(bx, w->y + 4, 22, 18, 4, mix(hi, 0xFFFFFF, 60));
    fill_rect(bx + 6, w->y + 14, 10, 2, RGB(20, 50, 80));
    /* content */
    int cx, cy, cw, ch;
    win_content(w, &cx, &cy, &cw, &ch);
    fill_rect(cx, cy, cw, ch, RGB(244, 246, 249));
    Rect saved = gfx_getclip(), c = { cx, cy, cw, ch };
    gfx_setclip_r(rect_isect(saved, c));
    w->app->draw(w, cx, cy, cw, ch);
    gfx_setclip_r(saved);
}

static void fmt_clock(char *t, char *d) {
    if (g_show_secs) ksnprintf(t, 16, "%02d:%02d:%02d", now.hour, now.min, now.sec);
    else ksnprintf(t, 16, "%02d:%02d", now.hour, now.min);
    ksnprintf(d, 16, "%02d.%02d.%04d", now.day, now.mon, now.year);
}

static void draw_taskbar(void) {
    int y = SH - TASK_H;
    gradient_v(0, y, SW, TASK_H, RGB(24, 48, 82), RGB(10, 24, 46));
    hline(0, y, SW, g_acc_hi);
    Rect sb = start_btn();
    fill_rrect(sb.x, sb.y, sb.w, sb.h, 6, start_open ? g_acc_lo : g_acc_hi);
    fill_rrect(sb.x + 1, sb.y + 1, sb.w - 2, sb.h / 2, 5, mix(start_open ? g_acc_lo : g_acc_hi, 0xFFFFFF, 50));
    fill_circle(sb.x + 18, sb.y + sb.h / 2 + 1, 8, RGB(235, 250, 255));
    fill_circle(sb.x + 16, sb.y + sb.h / 2 - 1, 3, RGB(255, 255, 255));
    circle(sb.x + 18, sb.y + sb.h / 2 + 1, 8, RGB(20, 90, 130));
    draw_text(sb.x + 34, sb.y + (sb.h - 16) / 2, "Aqua", RGB(255, 255, 255));
    int n = task_count(), slot = 0, fi = focus_idx();
    for (int i = 0; i < g_napps; i++) {
        if (!wins[i].open) continue;
        Rect r = task_rect(slot++, n);
        int act = (i == fi);
        fill_rrect(r.x, r.y, r.w, r.h, 5, act ? RGB(90, 130, 175) : RGB(44, 70, 106));
        if (act) hline(r.x + 4, r.y + r.h - 2, r.w - 8, g_acc_hi);
        draw_icon(wins[i].app->icon, r.x + 5, r.y + 6, 20);
        int maxc = (r.w - 34) / 8;
        char nm[24]; scopy(nm, wins[i].app->name, maxc + 1 > 23 ? 23 : maxc + 1);
        draw_text(r.x + 30, r.y + (r.h - 16) / 2, nm, wins[i].min ? RGB(160, 180, 205) : RGB(255, 255, 255));
    }
    Rect t = tray_rect();
    fill_rrect(t.x + 4, t.y + 4, t.w - 8, t.h - 8, 5, RGB(16, 34, 62));
    char ts[16], ds[16]; fmt_clock(ts, ds);
    draw_text(t.x + 10, t.y + 6, ts, RGB(255, 255, 255));
    draw_text(t.x + 10, t.y + 21, ds, RGB(170, 195, 225));
    draw_text(t.x + t.w - 10 - 6 * 8, t.y + 6, ARCH_BITS == 64 ? "64-bit" : "32-bit", g_acc_hi);
    draw_text(t.x + t.w - 10 - 6 * 8, t.y + 21, "AquaOS", RGB(120, 150, 190));
}

static void draw_menu(void) {
    Rect m = menu_rect();
    fill_rect_alpha(m.x + 5, m.y + 5, m.w, m.h, 0, 70);
    fill_rrect(m.x, m.y, m.w, m.h, 8, RGB(22, 40, 70));
    fill_rrect(m.x + 1, m.y + 1, m.w - 2, m.h - 2, 8, RGB(236, 242, 248));
    gradient_v(m.x + 1, m.y + 1, m.w - 2, MENU_HEAD - 1, g_acc_hi, g_acc_lo);
    draw_text_s(m.x + 14, m.y + 11, "AquaOS", RGB(255, 255, 255), 1);
    draw_text(m.x + 14 + 8 * 7, m.y + 11, AQUA_VERSION, RGB(220, 240, 255));
    draw_text(m.x + m.w - 8 - 8 * 7, m.y + 11, ARCH_BITS == 64 ? "x86_64" : "i386", RGB(220, 240, 255));
    for (int i = 0; i < g_napps; i++) {
        int iy = m.y + MENU_HEAD + i * MENU_ITEM;
        if (i == menu_hover) fill_rect(m.x + 3, iy, m.w - 6, MENU_ITEM, mix(g_acc_hi, 0xFFFFFF, 90));
        draw_icon(g_apps[i]->icon, m.x + 12, iy + 3, 24);
        draw_text(m.x + 48, iy + 7, g_apps[i]->name, RGB(20, 30, 45));
    }
    int sy = m.y + MENU_HEAD + g_napps * MENU_ITEM + 5;
    hline(m.x + 10, sy, m.w - 20, RGB(170, 182, 196));
    const char *pw[2] = { "Restart", "Shut down" };
    for (int k = 0; k < 2; k++) {
        int iy = sy + 7 + k * MENU_ITEM;
        if (menu_hover == g_napps + k) fill_rect(m.x + 3, iy, m.w - 6, MENU_ITEM, mix(RGB(230, 90, 90), 0xFFFFFF, 80));
        fill_circle(m.x + 24, iy + 15, 8, k ? RGB(220, 70, 70) : RGB(240, 170, 50));
        fill_circle(m.x + 24, iy + 15, 4, RGB(236, 242, 248));
        draw_text(m.x + 48, iy + 7, pw[k], RGB(20, 30, 45));
    }
}

int wm_compose_pending(void) { return dirty_v; }

void wm_compose(void) {
    if (!dirty_v) return;
    Rect d = dirty; dirty_v = 0;
    gfx_target(g_back, SW);
    gfx_setclip_r(d);
    for (int y = d.y; y < d.y + d.h; y++) memcpy32(g_back + y * SW + d.x, g_wall + y * SW + d.x, d.w);
    for (int i = 0; i < g_napps; i++) {
        Rect r = icon_rect(i);
        Rect rr = rect_isect(r, d);
        if (rr.w <= 0 || rr.h <= 0) continue;
        if (i == sel_icon) fill_rect_alpha(r.x, r.y, r.w, r.h, g_acc_hi, 110);
        draw_icon(g_apps[i]->icon, r.x + (r.w - 44) / 2, r.y + 6, 44);
        const char *nm = g_apps[i]->name;
        int tx = r.x + (r.w - text_w(nm)) / 2;
        draw_text(tx + 1, r.y + 58, nm, RGB(0, 20, 40));
        draw_text(tx, r.y + 57, nm, RGB(255, 255, 255));
    }
    int fi = focus_idx();
    for (int i = 0; i < nz; i++) {
        Window *w = &wins[zord[i]];
        if (!w->open || w->min) continue;
        Rect wr = { w->x, w->y, w->w + 8, w->h + 8 };
        Rect ri = rect_isect(wr, d);
        if (ri.w <= 0 || ri.h <= 0) continue;
        draw_window(w, zord[i] == fi);
    }
    draw_taskbar();
    if (start_open) draw_menu();
    gfx_clipreset();
    gfx_blit(d.x, d.y, d.w, d.h);
}

/* ------------------------------------------------------------------ input */
static void menu_activate(int it) {
    start_open = 0;
    wm_inval_all();
    if (it < 0) return;
    if (it < g_napps) wm_open(it);
    else if (it == g_napps) wm_reboot_prompt(0);
    else wm_reboot_prompt(1);
}

static void send_content_mouse(Window *w, int mx, int my, int btn, int ev) {
    int cx, cy, cw, ch; win_content(w, &cx, &cy, &cw, &ch);
    if (w->app->mouse) w->app->mouse(w, mx - cx, my - cy, btn, ev);
}

void wm_mouse(int mx, int my, int btn, int prev) {
    int ldown = (btn & 1) && !(prev & 1), lup = !(btn & 1) && (prev & 1);
    int rdown = (btn & 2) && !(prev & 2);

    if (drag_win >= 0) {
        if (btn & 1) {
            Window *w = &wins[drag_win];
            int nx = mx - drag_dx, ny = my - drag_dy;
            if (ny < 0) ny = 0;
            if (ny > SH - TASK_H - TITLE_H) ny = SH - TASK_H - TITLE_H;
            if (nx < 60 - w->w) nx = 60 - w->w;
            if (nx > SW - 60) nx = SW - 60;
            if (nx != w->x || ny != w->y) { wm_inval_win(w); w->x = nx; w->y = ny; wm_inval_win(w); }
        }
        if (lup) drag_win = -1;
        return;
    }
    if (cap_win >= 0) {
        Window *w = &wins[cap_win];
        if (lup) { send_content_mouse(w, mx, my, 1, EV_UP); cap_win = -1; }
        else if (btn & 1) send_content_mouse(w, mx, my, 1, EV_MOVE);
        return;
    }

    /* start menu hover */
    if (start_open) {
        int h = menu_item_at(mx, my);
        if (h != menu_hover) { menu_hover = h; Rect m = menu_rect(); wm_inval(m.x, m.y, m.w + 8, m.h + 8); }
    }
    if (!ldown && !rdown) return;

    /* ---- start menu click ---- */
    if (start_open) {
        Rect m = menu_rect(), sb = start_btn();
        if (rect_hit(m, mx, my)) { menu_activate(menu_item_at(mx, my)); return; }
        start_open = 0; menu_hover = -1; wm_inval_all();
        if (rect_hit(sb, mx, my)) return;
    }
    /* ---- taskbar ---- */
    if (my >= SH - TASK_H) {
        if (rect_hit(start_btn(), mx, my)) { start_open = !start_open; menu_hover = -1; wm_inval_all(); return; }
        int n = task_count(), slot = 0, fi = focus_idx();
        for (int i = 0; i < g_napps; i++) {
            if (!wins[i].open) continue;
            if (rect_hit(task_rect(slot++, n), mx, my)) {
                if (i == fi) { wins[i].min = 1; wm_inval_win(&wins[i]); }
                else { wins[i].min = 0; z_raise(i); wm_inval_win(&wins[i]); }
                wm_inval(0, SH - TASK_H, SW, TASK_H);
                return;
            }
        }
        return;
    }
    /* ---- windows (top to bottom) ---- */
    for (int i = nz - 1; i >= 0; i--) {
        Window *w = &wins[zord[i]];
        if (!w->open || w->min) continue;
        Rect r = { w->x, w->y, w->w, w->h };
        if (!rect_hit(r, mx, my)) continue;
        int idx = zord[i];
        if (idx != focus_idx()) { z_raise(idx); wm_inval_win(w); wm_inval(0, SH - TASK_H, SW, TASK_H); }
        if (my < w->y + TITLE_H) {
            int bx = w->x + w->w - 26;
            if (mx >= bx && mx < bx + 22 && ldown) { wm_close(w); return; }
            if (mx >= bx - 25 && mx < bx - 3 && ldown) { win_toggle_max(w); return; }
            if (mx >= bx - 50 && mx < bx - 28 && ldown) { w->min = 1; wm_inval_win(w); wm_inval(0, SH - TASK_H, SW, TASK_H); return; }
            if (ldown) {
                if (dbl_id == 1000 + idx && g_ms - dbl_ms < 400) { win_toggle_max(w); dbl_id = -99; return; }
                dbl_id = 1000 + idx; dbl_ms = g_ms;
                if (!w->max) { drag_win = idx; drag_dx = mx - w->x; drag_dy = my - w->y; }
            }
            return;
        }
        int ev = EV_DOWN, b = rdown ? 2 : 1;
        if (ldown && dbl_id == idx && g_ms - dbl_ms < 400 && abs(mx - dbl_x) < 6 && abs(my - dbl_y) < 6) { ev = EV_DBL; dbl_id = -99; }
        else if (ldown) { dbl_id = idx; dbl_ms = g_ms; dbl_x = mx; dbl_y = my; }
        send_content_mouse(w, mx, my, b, ev);
        if (ldown) cap_win = idx;
        wm_inval_win(w);
        return;
    }
    /* ---- desktop ---- */
    int hit = -1;
    for (int i = 0; i < g_napps; i++) if (rect_hit(icon_rect(i), mx, my)) hit = i;
    if (hit >= 0 && ldown) {
        if (dbl_id == 2000 + hit && g_ms - dbl_ms < 450) { dbl_id = -99; sel_icon = -1; wm_open(hit); wm_inval_all(); return; }
        dbl_id = 2000 + hit; dbl_ms = g_ms;
    }
    if (sel_icon != hit) {
        if (sel_icon >= 0) { Rect r = icon_rect(sel_icon); wm_inval(r.x, r.y, r.w, r.h); }
        sel_icon = hit;
        if (hit >= 0) { Rect r = icon_rect(hit); wm_inval(r.x, r.y, r.w, r.h); }
    }
}

void wm_key(int key) {
    if (key == K_WIN || (kmod_ctrl && key == 27)) { start_open = !start_open; menu_hover = -1; wm_inval_all(); return; }
    if (key == 27 && start_open) { start_open = 0; wm_inval_all(); return; }
    int fi = focus_idx();
    if (kmod_alt && key == '\t') {
        if (nz < 2) return;
        int target = -1;
        for (int i = 0; i < nz; i++) if (wins[zord[i]].open) { target = zord[i]; break; }
        if (target >= 0) { wins[target].min = 0; z_raise(target); wm_inval_all(); }
        return;
    }
    if (kmod_alt && key == K_F4 && fi >= 0) { wm_close(&wins[fi]); return; }
    if (fi >= 0 && wins[fi].app->key) { wins[fi].app->key(&wins[fi], key); wm_inval_win(&wins[fi]); }
}

void wm_tick(void) {
    if (g_ms - last_clock >= 500) {
        last_clock = g_ms;
        RTime t; rtc_read(&t);
        int changed = g_show_secs ? t.sec != now.sec : t.min != now.min || t.day != now.day;
        now = t;
        if (changed) { Rect r = tray_rect(); wm_inval(r.x, r.y, r.w, r.h); }
    }
    for (int i = 0; i < g_napps; i++)
        if (wins[i].open && wins[i].app->tick) wins[i].app->tick(&wins[i]);
}

void wm_reboot_prompt(int shutdown) {
    gfx_target(g_back, SW); gfx_clipreset();
    fill_rect(0, 0, SW, SH, RGB(6, 22, 46));
    draw_text_s(SW / 2 - 6 * 8 * 3 / 2 - 12, SH / 2 - 60, "AquaOS", RGB(120, 220, 245), 3);
    const char *m = shutdown ? "Shutting down..." : "Restarting...";
    draw_text(SW / 2 - text_w(m) / 2, SH / 2, m, RGB(220, 235, 250));
    gfx_blit_all();
    delay_ms(1500);
    if (shutdown) {
        fill_rect(0, 0, SW, SH, 0x000000);
        const char *s = "It is now safe to turn off your computer.";
        draw_text(SW / 2 - text_w(s) / 2, SH / 2 - 8, s, RGB(255, 170, 60));
        gfx_blit_all();
        sys_shutdown();
    }
    sys_reboot();
}
