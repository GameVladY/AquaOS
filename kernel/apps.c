/* AquaOS - applications part 1: Terminal, Notepad, Files, plus the app registry */
#include "aqua.h"

extern const App app_terminal, app_notepad, app_files, app_calc, app_paint, app_snake, app_clock, app_sysinfo, app_settings;
const App *g_apps[16];
int g_napps;

void apps_register(void) {
    g_apps[0] = &app_terminal; g_apps[1] = &app_notepad; g_apps[2] = &app_files;
    g_apps[3] = &app_calc;     g_apps[4] = &app_paint;   g_apps[5] = &app_snake;
    g_apps[6] = &app_clock;    g_apps[7] = &app_sysinfo; g_apps[8] = &app_settings;
    g_napps = 9;
}

/* ====================================================================== */
/*  Terminal                                                               */
/* ====================================================================== */
#define TCOLS 80
#define TLINES 300
static char tch[TLINES][TCOLS];
static u8 tcl[TLINES][TCOLS];
static int tline, tcol, tscroll, tinited, tlast_blink;
static char tin[96]; static int tinlen;
static char thist[12][96]; static int thn, thpos;
static const u32 tpal[] = { RGB(205, 225, 235), RGB(90, 255, 150), RGB(90, 210, 255), RGB(255, 110, 110),
                            RGB(255, 220, 100), RGB(255, 255, 255), RGB(140, 155, 180) };
enum { C_DEF, C_GRN, C_CYN, C_RED, C_YEL, C_WHT, C_GRY };

static void t_clear_row(int n) { memset(tch[n % TLINES], ' ', TCOLS); memset(tcl[n % TLINES], 0, TCOLS); }
static void t_newline(void) { tline++; tcol = 0; tscroll = 0; t_clear_row(tline); }
static void t_putc(char c, int col) {
    if (c == '\n') { t_newline(); return; }
    if (c == '\r') { tcol = 0; return; }
    if (c == '\t') { do t_putc(' ', col); while (tcol & 3); return; }
    if (tcol >= TCOLS) t_newline();
    tch[tline % TLINES][tcol] = c; tcl[tline % TLINES][tcol] = col; tcol++;
}
static void t_puts(const char *s, int col) { while (*s) t_putc(*s++, col); }
static void t_printf(int col, const char *fmt, ...) {
    char b[256]; va_list ap; va_start(ap, fmt); kvsnprintf(b, sizeof b, fmt, ap); va_end(ap); t_puts(b, col);
}
static void t_prompt(void) { t_puts("aqua@os", C_GRN); t_puts(":~$ ", C_CYN); }
static void t_clear(void) { tline = 0; tcol = 0; tscroll = 0; for (int i = 0; i < TLINES; i++) t_clear_row(i); }

/* tiny integer expression evaluator for the `calc` command */
static const char *cp; static int cerr;
static void cskip(void) { while (*cp == ' ') cp++; }
static i32 c_expr(void);
static i32 c_atom(void) {
    cskip();
    if (*cp == '(') { cp++; i32 v = c_expr(); cskip(); if (*cp == ')') cp++; else cerr = 1; return v; }
    if (*cp == '-') { cp++; return -c_atom(); }
    if (*cp >= '0' && *cp <= '9') { i32 v = 0; while (*cp >= '0' && *cp <= '9') v = v * 10 + (*cp++ - '0'); return v; }
    cerr = 1; return 0;
}
static i32 c_term(void) {
    i32 v = c_atom();
    for (;;) {
        cskip(); char o = *cp;
        if (o == '*') { cp++; v *= c_atom(); }
        else if (o == '/' || o == '%') {
            cp++; i32 r = c_atom();
            if (r == 0) { cerr = 2; return 0; }
            if (r == -1) v = o == '/' ? -v : 0; else v = o == '/' ? v / r : v % r;
        } else break;
    }
    return v;
}
static i32 c_expr(void) {
    i32 v = c_term();
    for (;;) {
        cskip(); char o = *cp;
        if (o == '+') { cp++; v += c_term(); } else if (o == '-') { cp++; v -= c_term(); } else break;
    }
    return v;
}

static const char *wdays[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

static void t_features(void) {
    struct { u32 bit; const char *n; } f[] = { { 0, "fpu" }, { 4, "tsc" }, { 5, "msr" }, { 6, "pae" }, { 9, "apic" },
        { 15, "cmov" }, { 23, "mmx" }, { 25, "sse" }, { 26, "sse2" } };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) if (g_cpu.feat_edx & (1u << f[i].bit)) t_printf(C_DEF, "%s ", f[i].n);
    if (g_cpu.feat_ecx & 1) t_puts("sse3 ", C_DEF);
    if (g_cpu.ext_edx & (1u << 29)) t_puts("lm(64-bit) ", C_GRN);
    t_putc('\n', C_DEF);
}

static void t_memmap(void) {
    E820Entry *e = (E820Entry *)(uintptr_t)BI->e820_addr;
    static const char *tn[] = { "?", "usable", "reserved", "ACPI reclaim", "ACPI NVS", "bad" };
    for (u32 i = 0; i < BI->e820_count; i++)
        t_printf(C_DEF, " %08x%08x  %6u KiB  %s\n", e[i].base_hi, e[i].base_lo, (e[i].len_hi << 22) + (e[i].len_lo >> 10),
                 e[i].type < 6 ? tn[e[i].type] : "?");
}

static void t_fetch(void) {
    static const char *logo[7] = { "      /\\      ", "     /  \\     ", "    /    \\    ", "   |  ()  |   ", "   |      |   ", "    \\    /    ", "     `--'     " };
    char info[7][64];
    u32 ms = g_ms, s = ms / 1000;
    ksnprintf(info[0], 64, "aqua@os");
    ksnprintf(info[1], 64, "-------");
    ksnprintf(info[2], 64, "OS:     AquaOS %s (%s)", AQUA_VERSION, ARCH_NAME);
    ksnprintf(info[3], 64, "CPU:    %s", g_cpu.brand);
    ksnprintf(info[4], 64, "Memory: %u MiB", mem_total_kb() / 1024);
    ksnprintf(info[5], 64, "Screen: %dx%d   Uptime: %um %us", SW, SH, s / 60, s % 60);
    ksnprintf(info[6], 64, "Shell:  aqua-sh   Apps: %d", g_napps);
    for (int i = 0; i < 7; i++) { t_puts(logo[i], C_CYN); t_puts(info[i], i == 0 ? C_GRN : C_DEF); t_putc('\n', C_DEF); }
}

static void t_help(void) {
    static const char *h[] = {
        "  help            this list           clear           clear the screen",
        "  ls / df         list files / usage  cat <f>         show a file",
        "  write <f> text  create/overwrite    append <f> text append to file",
        "  touch <f>       create empty file   rm <f>          delete a file",
        "  cp <a> <b>      copy a file         mv <a> <b>      rename a file",
        "  edit <f>        open in Notepad     open <app>      launch an app",
        "  apps            list applications   ps              open windows",
        "  calc <expr>     e.g. calc (2+3)*7   echo <text>     print text",
        "  date / uptime   clock and uptime    ver / uname     system version",
        "  cpu / mem       CPU and memory      memmap          BIOS memory map",
        "  aquafetch       system summary      theme <0-2> [0-3] wallpaper/accent",
        "  beep [hz] [ms]  PC speaker          reboot / shutdown",
    };
    for (unsigned i = 0; i < sizeof h / sizeof h[0]; i++) { t_puts(h[i], C_DEF); t_putc('\n', C_DEF); }
    t_puts("  Tips: Up/Down = history, PgUp/PgDn = scrollback, Ctrl+L = clear\n", C_GRY);
}

static int find_app(const char *n) {
    for (int i = 0; i < g_napps; i++) {
        const char *a = g_apps[i]->name; int j = 0, ok = 1;
        while (n[j]) { char x = n[j], y = a[j]; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) { ok = 0; break; } j++; }
        if (ok && j > 0) return i;
    }
    return -1;
}

static void term_exec(char *line) {
    char *argv[8]; int argc = 0; char *p = line;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        if (argc < 8) argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = 0;
    }
    if (!argc) return;
    const char *c = argv[0];
    char rest[96]; rest[0] = 0;
    for (int i = (c[0] == 'w' || c[0] == 'a') ? 2 : 1; i < argc; i++) {
        if (rest[0]) scopy(rest + strlen(rest), " ", 2);
        scopy(rest + strlen(rest), argv[i], (int)(sizeof rest - strlen(rest)));
    }
    if (!strcmp(c, "help") || !strcmp(c, "?")) t_help();
    else if (!strcmp(c, "clear") || !strcmp(c, "cls")) t_clear();
    else if (!strcmp(c, "echo")) { t_puts(rest, C_DEF); t_putc('\n', C_DEF); }
    else if (!strcmp(c, "ver") || !strcmp(c, "version")) t_printf(C_DEF, "AquaOS %s (%s) built %s %s\n", AQUA_VERSION, ARCH_NAME, __DATE__, __TIME__);
    else if (!strcmp(c, "uname")) t_printf(C_DEF, "AquaOS aquaos %s %s\n", AQUA_VERSION, ARCH_NAME);
    else if (!strcmp(c, "whoami")) t_puts("aqua\n", C_DEF);
    else if (!strcmp(c, "hostname")) t_puts("aquaos\n", C_DEF);
    else if (!strcmp(c, "pwd")) t_puts("/\n", C_DEF);
    else if (!strcmp(c, "date") || !strcmp(c, "time")) {
        RTime t; rtc_read(&t);
        t_printf(C_DEF, "%s %02d.%02d.%04d %02d:%02d:%02d\n", wdays[t.wday % 7], t.day, t.mon, t.year, t.hour, t.min, t.sec);
    }
    else if (!strcmp(c, "uptime")) { u32 s = g_ms / 1000; t_printf(C_DEF, "up %u h %u min %u s\n", s / 3600, s / 60 % 60, s % 60); }
    else if (!strcmp(c, "mem")) {
        u32 kb = mem_total_kb();
        t_printf(C_DEF, "Usable RAM : %u MiB (%u KiB)\n", kb / 1024, kb);
        t_printf(C_DEF, "Kernel size: %u KiB (image + static buffers)\n", ((u32)(uintptr_t)__bss_end - 0x100000) / 1024);
        t_printf(C_DEF, "RAM disk   : %d files, %d bytes\n", fs_count(), fs_total_bytes());
    }
    else if (!strcmp(c, "cpu")) {
        t_printf(C_DEF, "Vendor : %s\nModel  : %s\nFamily : %u  Model: %u  Stepping: %u\nMode   : %d-bit kernel\nFlags  : ", g_cpu.vendor, g_cpu.brand, g_cpu.family, g_cpu.model, g_cpu.stepping, ARCH_BITS);
        t_features();
    }
    else if (!strcmp(c, "memmap")) t_memmap();
    else if (!strcmp(c, "aquafetch") || !strcmp(c, "fetch") || !strcmp(c, "neofetch")) t_fetch();
    else if (!strcmp(c, "ls") || !strcmp(c, "dir")) {
        int n = 0;
        for (int i = 0; i < FS_MAX; i++) if (g_fs[i].used) { t_printf(C_DEF, "%-24s %6d B\n", g_fs[i].name, g_fs[i].size); n++; }
        if (!n) t_puts("(no files)\n", C_GRY);
    }
    else if (!strcmp(c, "df")) t_printf(C_DEF, "ramdisk: %d/%d files, %d bytes used (volatile - cleared on reboot)\n", fs_count(), FS_MAX, fs_total_bytes());
    else if (!strcmp(c, "cat") && argc > 1) {
        FsFile *f = fs_find(argv[1]);
        if (!f) t_printf(C_RED, "cat: %s: no such file\n", argv[1]);
        else { for (int i = 0; i < f->size; i++) t_putc(f->data[i], C_DEF); if (f->size && f->data[f->size - 1] != '\n') t_putc('\n', C_DEF); }
    }
    else if (!strcmp(c, "write") && argc > 1) { if (fs_write(argv[1], rest, (int)strlen(rest)) < 0) t_puts("write: failed\n", C_RED); }
    else if (!strcmp(c, "append") && argc > 1) { fs_append(argv[1], rest, (int)strlen(rest)); fs_append(argv[1], "\n", 1); }
    else if (!strcmp(c, "touch") && argc > 1) { if (!fs_create(argv[1])) t_puts("touch: failed\n", C_RED); }
    else if (!strcmp(c, "rm") && argc > 1) { if (fs_delete(argv[1]) < 0) t_printf(C_RED, "rm: %s: no such file\n", argv[1]); }
    else if (!strcmp(c, "cp") && argc > 2) {
        FsFile *f = fs_find(argv[1]);
        if (!f) t_printf(C_RED, "cp: %s: no such file\n", argv[1]); else if (fs_write(argv[2], f->data, f->size) < 0) t_puts("cp: failed\n", C_RED);
    }
    else if (!strcmp(c, "mv") && argc > 2) {
        FsFile *f = fs_find(argv[1]);
        if (!f) t_printf(C_RED, "mv: %s: no such file\n", argv[1]);
        else if (fs_write(argv[2], f->data, f->size) >= 0) fs_delete(argv[1]); else t_puts("mv: failed\n", C_RED);
    }
    else if (!strcmp(c, "edit") && argc > 1) notepad_open(argv[1]);
    else if (!strcmp(c, "apps")) { for (int i = 0; i < g_napps; i++) t_printf(C_DEF, "  %d  %s\n", i, g_apps[i]->name); }
    else if (!strcmp(c, "ps")) {
        t_puts("  ID  WINDOW         STATE\n", C_GRY);
        for (int i = 0; i < g_napps; i++) { Window *w = wm_win(i); if (w->open) t_printf(C_DEF, "  %-3d %-14s %s\n", i, g_apps[i]->name, w->min ? "minimized" : "running"); }
    }
    else if (!strcmp(c, "open") && argc > 1) {
        int i = find_app(argv[1]);
        if (i < 0) t_printf(C_RED, "open: unknown app '%s' (try: apps)\n", argv[1]); else wm_open(i);
    }
    else if (!strcmp(c, "calc") && argc > 1) {
        char e[96]; e[0] = 0;
        for (int i = 1; i < argc; i++) scopy(e + strlen(e), argv[i], (int)(sizeof e - strlen(e)));
        cp = e; cerr = 0; i32 v = c_expr(); cskip(); if (*cp) cerr = 1;
        if (cerr == 2) t_puts("calc: division by zero\n", C_RED); else if (cerr) t_puts("calc: syntax error\n", C_RED); else t_printf(C_GRN, "= %d\n", v);
    }
    else if (!strcmp(c, "beep")) { speaker(argc > 1 ? katoi(argv[1]) : 880, argc > 2 ? katoi(argv[2]) : 200); }
    else if (!strcmp(c, "theme") && argc > 1) {
        int th = katoi(argv[1]), ac = argc > 2 ? katoi(argv[2]) : g_accent;
        if (th < 0 || th > 2 || ac < 0 || ac > 3) t_puts("theme: wallpaper 0-2, accent 0-3\n", C_RED); else wm_set_theme(th, ac);
    }
    else if (!strcmp(c, "reboot") || !strcmp(c, "restart")) wm_reboot_prompt(0);
    else if (!strcmp(c, "shutdown") || !strcmp(c, "poweroff") || !strcmp(c, "halt")) wm_reboot_prompt(1);
    else t_printf(C_RED, "aqua-sh: %s: command not found (try 'help')\n", c);
}

static void term_init(Window *w) {
    (void)w;
    if (tinited) return;
    tinited = 1; t_clear();
    t_puts("AquaOS Terminal  -  type 'help' for commands\n", C_CYN);
    t_printf(C_GRY, "Kernel %s build, %d apps, RAM disk ready.\n\n", ARCH_NAME, g_napps);
    t_prompt();
}
static void term_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    fill_rect(cx, cy, cw, ch, RGB(12, 16, 24));
    int rows = ch / 16, cols = (cw - 8) / 8;
    if (cols > TCOLS) cols = TCOLS;
    int last = tline - tscroll, first = last - rows + 1;
    if (first < 0) first = 0;
    if (tline - first >= TLINES) first = tline - TLINES + 1;
    for (int r = 0; r < rows; r++) {
        int ln = first + r;
        if (ln > last) break;
        int y = cy + r * 16;
        for (int c = 0; c < cols; c++) {
            char ch2 = tch[ln % TLINES][c];
            if (ch2 != ' ') draw_char(cx + 4 + c * 8, y, ch2, tpal[tcl[ln % TLINES][c]]);
        }
    }
    if (!tscroll && ((g_ms / 500) & 1)) {
        int r = tline - first;
        if (r >= 0 && r < rows && tcol < cols) fill_rect(cx + 4 + tcol * 8, cy + r * 16 + 13, 8, 2, RGB(90, 255, 150));
    }
    if (tscroll) draw_text(cx + cw - 8 * 12 - 4, cy + 2, "[scrollback]", RGB(255, 220, 100));
}
static void term_key(Window *w, int k) {
    (void)w;
    if (kmod_ctrl && (k == 'l' || k == 'L')) { t_clear(); t_prompt(); tinlen = 0; return; }
    if (kmod_ctrl && (k == 'c' || k == 'C')) { t_puts("^C\n", C_GRY); tinlen = 0; t_prompt(); return; }
    if (k == K_PGUP) { tscroll += 8; if (tscroll > tline) tscroll = tline; if (tscroll > TLINES - 40) tscroll = TLINES - 40; return; }
    if (k == K_PGDN) { tscroll -= 8; if (tscroll < 0) tscroll = 0; return; }
    tscroll = 0;
    if (k == '\n') {
        t_putc('\n', C_DEF); tin[tinlen] = 0;
        if (tinlen) { scopy(thist[thn % 12], tin, 96); thn++; thpos = thn; }
        char line[96]; scopy(line, tin, 96); tinlen = 0;
        term_exec(line);
        if (tcol) t_putc('\n', C_DEF);
        t_prompt();
    } else if (k == '\b') {
        if (tinlen > 0) { tinlen--; tcol--; tch[tline % TLINES][tcol] = ' '; }
    } else if (k == K_UP || k == K_DOWN) {
        if (k == K_UP && thpos > 0 && thn - thpos < 12) thpos--;
        else if (k == K_DOWN && thpos < thn) thpos++;
        else return;
        while (tinlen > 0) { tinlen--; tcol--; tch[tline % TLINES][tcol] = ' '; }
        if (thpos < thn) { scopy(tin, thist[thpos % 12], 96); tinlen = (int)strlen(tin); for (int i = 0; i < tinlen; i++) t_putc(tin[i], C_DEF); }
    } else if (k >= 32 && k < 127 && tinlen < 66) { tin[tinlen++] = (char)k; t_putc((char)k, C_DEF); }
}
static void term_tick(Window *w) {
    int b = g_ms / 500;
    if (b != tlast_blink) { tlast_blink = b; wm_inval_win(w); }
}
const App app_terminal = { "Terminal", "AquaOS Terminal", 0, 648, 416, term_init, term_draw, 0, term_key, term_tick, 0 };

/* ====================================================================== */
/*  Notepad                                                                */
/* ====================================================================== */
static char nbuf[FS_FILESZ + 1];
static int nlen, ncur, ntop, nmod, nedit_name;
static char nname[28] = "untitled.txt";
static char nmsg[40]; static u32 nmsg_ms;
static int nblink;

static void note_save(void) {
    if (fs_write(nname, nbuf, nlen) >= 0) { nmod = 0; scopy(nmsg, "Saved to RAM disk", 40); } else scopy(nmsg, "Save failed (disk full?)", 40);
    nmsg_ms = g_ms ? g_ms : 1;
}
static void note_new(void) { nlen = ncur = ntop = 0; nmod = 0; nbuf[0] = 0; scopy(nname, "untitled.txt", 28); }
void notepad_open(const char *name) {
    Window *w = wm_open(1);
    (void)w;
    FsFile *f = fs_find(name);
    nlen = ncur = ntop = 0; nmod = 0;
    scopy(nname, name, 28);
    if (f) { nlen = f->size; memcpy(nbuf, f->data, nlen); }
    nbuf[nlen] = 0;
    wm_inval_win(w);
}
static void note_init(Window *w) { (void)w; if (!nname[0]) note_new(); }

static int note_line_start(int line) {
    int i = 0, l = 0;
    while (l < line && i < nlen) { if (nbuf[i] == '\n') l++; i++; }
    return i;
}
static void note_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    gradient_v(cx, cy, cw, 30, RGB(232, 238, 244), RGB(210, 220, 230));
    ui_button(cx + 6, cy + 4, 50, 22, "New", 0);
    ui_button(cx + 60, cy + 4, 56, 22, "Save", 0);
    fill_rect(cx + 124, cy + 4, cw - 132, 22, nedit_name ? RGB(255, 255, 230) : RGB(250, 252, 254));
    rect_outline(cx + 124, cy + 4, cw - 132, 22, nedit_name ? RGB(230, 150, 30) : RGB(150, 162, 178));
    draw_text(cx + 130, cy + 7, nname, RGB(20, 30, 45));
    if (nedit_name && ((g_ms / 500) & 1)) fill_rect(cx + 130 + (int)strlen(nname) * 8, cy + 8, 2, 14, RGB(20, 30, 45));
    int top = cy + 30, rows = (ch - 30 - 20) / 16;
    fill_rect(cx, top, cw, ch - 50, 0xFFFFFF);
    int cl = 0, cc = 0;
    for (int i = 0; i < ncur; i++) { if (nbuf[i] == '\n') { cl++; cc = 0; } else cc++; }
    if (cl < ntop) ntop = cl;
    if (cl >= ntop + rows) ntop = cl - rows + 1;
    int line = 0, col = 0, x0 = cx + 6, maxcol = (cw - 12) / 8;
    for (int i = 0; i <= nlen; i++) {
        if (line >= ntop && line < ntop + rows) {
            int y = top + (line - ntop) * 16 + 1;
            if (i < nlen && nbuf[i] != '\n' && col < maxcol) draw_char(x0 + col * 8, y, nbuf[i], RGB(20, 30, 45));
            if (i == ncur && !nedit_name && ((g_ms / 500) & 1)) fill_rect(x0 + col * 8, y, 2, 15, RGB(20, 30, 45));
        }
        if (line >= ntop + rows && i > ncur) break;
        if (i < nlen) { if (nbuf[i] == '\n') { line++; col = 0; } else col++; }
    }
    int sy = cy + ch - 20;
    fill_rect(cx, sy, cw, 20, RGB(222, 229, 237)); hline(cx, sy, cw, RGB(170, 182, 196));
    char b[80];
    ksnprintf(b, sizeof b, "Ln %d, Col %d   %d chars   %s", cl + 1, cc + 1, nlen, nmod ? "[modified]" : "");
    draw_text(cx + 8, sy + 2, b, RGB(50, 65, 85));
    if (nmsg_ms && g_ms - nmsg_ms < 3000) draw_text(cx + cw - text_w(nmsg) - 8, sy + 2, nmsg, RGB(20, 120, 60));
}
static void note_mouse(Window *w, int mx, int my, int btn, int ev) {
    (void)w;
    if (ev != EV_DOWN && ev != EV_DBL) return;
    if (my < 30) {
        if (mx >= 6 && mx < 56 && my >= 4 && my < 26) { note_new(); nedit_name = 0; }
        else if (mx >= 60 && mx < 116 && my >= 4 && my < 26) note_save();
        else if (mx >= 124) nedit_name = 1;
        return;
    }
    nedit_name = 0;
    (void)btn;
    int line = ntop + (my - 30) / 16, col = (mx - 6 + 4) / 8;
    int i = note_line_start(line), c = 0;
    while (i < nlen && nbuf[i] != '\n' && c < col) { i++; c++; }
    ncur = i;
}
static void note_key(Window *w, int k) {
    (void)w;
    if (nedit_name) {
        int n = (int)strlen(nname);
        if (k == '\n' || k == '\t' || k == 27) nedit_name = 0;
        else if (k == '\b') { if (n) nname[n - 1] = 0; }
        else if (k > 32 && k < 127 && n < 26) { nname[n] = (char)k; nname[n + 1] = 0; }
        return;
    }
    if (kmod_ctrl && (k == 's' || k == 'S')) { note_save(); return; }
    if (kmod_ctrl && (k == 'n' || k == 'N')) { note_new(); return; }
    int ls, le;
    switch (k) {
    case K_LEFT: if (ncur > 0) ncur--; return;
    case K_RIGHT: if (ncur < nlen) ncur++; return;
    case K_HOME: while (ncur > 0 && nbuf[ncur - 1] != '\n') ncur--; return;
    case K_END: while (ncur < nlen && nbuf[ncur] != '\n') ncur++; return;
    case K_UP: case K_DOWN: {
        int col = 0, i = ncur; while (i > 0 && nbuf[i - 1] != '\n') { i--; col++; }
        ls = i;
        if (k == K_UP) {
            if (ls == 0) return;
            int pe = ls - 1, ps = pe; while (ps > 0 && nbuf[ps - 1] != '\n') ps--;
            ncur = ps + (col < pe - ps ? col : pe - ps);
        } else {
            le = ncur; while (le < nlen && nbuf[le] != '\n') le++;
            if (le >= nlen) return;
            int ns = le + 1, ne = ns; while (ne < nlen && nbuf[ne] != '\n') ne++;
            ncur = ns + (col < ne - ns ? col : ne - ns);
        }
        return;
    }
    case K_DEL: if (ncur < nlen) { memmove(nbuf + ncur, nbuf + ncur + 1, nlen - ncur); nlen--; nmod = 1; } return;
    case K_PGUP: for (int n = 0; n < 10; n++) note_key(w, K_UP); return;
    case K_PGDN: for (int n = 0; n < 10; n++) note_key(w, K_DOWN); return;
    }
    if (nlen >= FS_FILESZ - 1) return;
    if (k == '\b') { if (ncur > 0) { memmove(nbuf + ncur - 1, nbuf + ncur, nlen - ncur + 1); ncur--; nlen--; nmod = 1; } return; }
    if (k == '\t') { for (int n = 0; n < 4 && nlen < FS_FILESZ - 1; n++) { memmove(nbuf + ncur + 1, nbuf + ncur, nlen - ncur + 1); nbuf[ncur++] = ' '; nlen++; } nmod = 1; return; }
    if (k == '\n' || (k >= 32 && k < 127)) { memmove(nbuf + ncur + 1, nbuf + ncur, nlen - ncur + 1); nbuf[ncur++] = (char)k; nlen++; nmod = 1; }
}
static void note_tick(Window *w) { int b = g_ms / 500; if (b != nblink) { nblink = b; wm_inval_win(w); } if (nmsg_ms && g_ms - nmsg_ms > 3000) { nmsg_ms = 0; wm_inval_win(w); } }
const App app_notepad = { "Notepad", "Notepad", 1, 560, 380, note_init, note_draw, note_mouse, note_key, note_tick, 0 };

/* ====================================================================== */
/*  Files                                                                  */
/* ====================================================================== */
static int fsel = -1, fnew;
static int files_slot(int row) { int n = 0; for (int i = 0; i < FS_MAX; i++) if (g_fs[i].used) { if (n == row) return i; n++; } return -1; }
static void files_draw(Window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    gradient_v(cx, cy, cw, 32, RGB(232, 238, 244), RGB(210, 220, 230));
    ui_button(cx + 6, cy + 5, 60, 22, "Open", 0);
    ui_button(cx + 70, cy + 5, 70, 22, "Delete", 0);
    ui_button(cx + 144, cy + 5, 84, 22, "New file", 0);
    fill_rect(cx, cy + 32, cw, 20, RGB(200, 212, 225));
    draw_text(cx + 34, cy + 34, "Name", RGB(40, 55, 75));
    draw_text(cx + cw - 90, cy + 34, "Size", RGB(40, 55, 75));
    int row = 0;
    for (int i = 0; i < FS_MAX; i++) {
        if (!g_fs[i].used) continue;
        int y = cy + 52 + row * 22;
        if (y + 22 > cy + ch - 22) break;
        if (i == fsel) fill_rect(cx, y, cw, 22, mix(g_acc_hi, 0xFFFFFF, 110));
        else if (row & 1) fill_rect(cx, y, cw, 22, RGB(236, 241, 246));
        draw_icon(1, cx + 8, y + 2, 18);
        draw_text(cx + 34, y + 3, g_fs[i].name, RGB(20, 30, 45));
        char b[16]; ksnprintf(b, sizeof b, "%d B", g_fs[i].size);
        draw_text(cx + cw - 90, y + 3, b, RGB(60, 75, 95));
        row++;
    }
    int sy = cy + ch - 22;
    fill_rect(cx, sy, cw, 22, RGB(222, 229, 237)); hline(cx, sy, cw, RGB(170, 182, 196));
    char b[96]; ksnprintf(b, sizeof b, "%d files, %d bytes   (RAM disk: cleared on reboot)", fs_count(), fs_total_bytes());
    draw_text(cx + 8, sy + 3, b, RGB(50, 65, 85));
}
static void files_mouse(Window *w, int mx, int my, int btn, int ev) {
    (void)w; (void)btn;
    if (ev != EV_DOWN && ev != EV_DBL) return;
    if (my < 32) {
        if (my >= 5 && my < 27) {
            if (mx >= 6 && mx < 66 && fsel >= 0) notepad_open(g_fs[fsel].name);
            else if (mx >= 70 && mx < 140 && fsel >= 0) { fs_delete(g_fs[fsel].name); fsel = -1; }
            else if (mx >= 144 && mx < 228) {
                char nm[28];
                do { ksnprintf(nm, sizeof nm, "newfile%d.txt", ++fnew); } while (fs_find(nm));
                if (fs_create(nm)) notepad_open(nm);
            }
        }
        return;
    }
    if (my >= 52) {
        int s = files_slot((my - 52) / 22);
        fsel = s;
        if (ev == EV_DBL && s >= 0) notepad_open(g_fs[s].name);
    }
}
static void files_key(Window *w, int k) {
    (void)w;
    if (k == K_DEL && fsel >= 0) { fs_delete(g_fs[fsel].name); fsel = -1; }
    if ((k == '\n') && fsel >= 0) notepad_open(g_fs[fsel].name);
}
const App app_files = { "Files", "File Manager", 2, 480, 340, 0, files_draw, files_mouse, files_key, 0, 0 };
