/* AquaOS - kernel entry: verbose boot terminal, then the desktop */
#include "aqua.h"

int wm_compose_pending(void);

/* ------------------------------------------------------------------ boot console */
static int bx, by;                       /* pixel cursor */
static int fast_boot, input_ready;
#define COL_TXT  RGB(205, 215, 225)
#define COL_DIM  RGB(120, 135, 150)
#define COL_OK   RGB(80, 255, 130)
#define COL_CYAN RGB(90, 215, 255)
#define COL_WARN RGB(255, 210, 80)

static void con_scroll(void) {
    memcpy32(g_back, g_back + 16 * SW, (size_t)(SH - 16) * SW);
    by -= 16;
    memset32(g_back + by * SW, RGB(6, 10, 16), (size_t)(SH - by) * SW);
    gfx_blit_all();
}
static void con_nl(void) {
    gfx_blit(0, by, SW, 16);
    bx = 8; by += 16;
    if (by + 16 > SH) con_scroll();
}
static void con_puts(const char *s, u32 c) {
    for (; *s; s++) {
        if (*s == '\n') { con_nl(); continue; }
        if (bx + 8 > SW - 8) con_nl();
        draw_char(bx, by, *s, c); bx += 8;
    }
    gfx_blit(0, by, SW, 16);
}
static void con_printf(u32 c, const char *fmt, ...) {
    char b[200]; va_list ap; va_start(ap, fmt); kvsnprintf(b, sizeof b, fmt, ap); va_end(ap); con_puts(b, c);
}
static void bdelay(u32 ms) {
    if (fast_boot) ms /= 12;
    u32 t = g_ms;
    while (g_ms - t < ms) {
        if (input_ready) { input_poll(); if (key_get()) { if (!fast_boot) { fast_boot = 1; } } }
        __asm__ volatile("hlt");
    }
}
static void stamp(void) { con_printf(COL_DIM, "[%5u.%03u] ", g_ms / 1000, g_ms % 1000); }
static void klog(u32 c, const char *fmt, ...) {
    char b[200]; va_list ap; va_start(ap, fmt); kvsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    stamp(); con_puts(b, c); con_nl();
}
static void kstat(const char *status, u32 sc, const char *fmt, ...) {
    char b[200]; va_list ap; va_start(ap, fmt); kvsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    if (strlen(b) > 62) b[62] = 0;
    stamp(); con_puts(b, COL_TXT);
    int target = 8 + 8 * 76;
    if (bx < target) bx = target;
    con_puts("[", COL_DIM); con_puts(status, sc); con_puts("]", COL_DIM);
    con_nl();
}
static void kok(const char *fmt, ...) {
    char b[200]; va_list ap; va_start(ap, fmt); kvsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    kstat("  OK  ", COL_OK, "%s", b);
}
static void bar(const char *label, u32 total_ms) {
    stamp(); con_puts(label, COL_TXT);
    int x0 = bx + 12, w = 260, y = by + 2;
    fill_rect(x0 - 1, y - 1, w + 2, 14, COL_DIM);
    fill_rect(x0, y, w, 12, RGB(10, 20, 30));
    int steps = 40;
    for (int i = 1; i <= steps; i++) {
        fill_rect(x0, y, w * i / steps, 12, mix(RGB(30, 150, 200), RGB(100, 255, 200), i * 256 / steps));
        gfx_blit(0, by, SW, 16);
        bdelay(total_ms / steps);
    }
    bx = x0 + w + 12;
    con_puts("done", COL_OK); con_nl();
}

/* ------------------------------------------------------------------ files that ship on the RAM disk */
static const char *readme =
"Welcome to AquaOS " AQUA_VERSION "!\n"
"\n"
"AquaOS is a small operating system written from scratch:\n"
"its own boot loader, its own 32-bit and 64-bit kernels,\n"
"its own drivers, window manager and applications.\n"
"No Linux, no borrowed kernel.\n"
"\n"
"GETTING AROUND\n"
"  - Double-click a desktop icon, or use the Aqua menu.\n"
"  - Drag title bars to move windows; use the buttons to\n"
"    minimize, maximize or close them.\n"
"  - Alt+Tab switches windows, Alt+F4 closes one,\n"
"    the Windows key opens the Aqua menu.\n"
"\n"
"APPS\n"
"  Terminal, Notepad, Files, Calculator, Paint, Snake,\n"
"  Clock & Calendar, System Info and Settings.\n"
"\n"
"Files live on a RAM disk: they work like a normal disk\n"
"while the computer is on, but are cleared on reboot.\n";

static const char *about =
"AquaOS - architecture notes\n"
"---------------------------\n"
"boot/boot.asm   BIOS (El Torito) boot loader: A20, E820 memory\n"
"                map, CPUID long-mode test, VBE video mode,\n"
"                protected mode, long mode + paging for 64-bit.\n"
"kernel/         freestanding C kernel: IDT, PIC, PIT timer,\n"
"                RTC, PS/2 keyboard and mouse, software\n"
"                renderer, window manager, RAM filesystem, apps.\n"
"The loader picks the 64-bit kernel when the CPU supports long\n"
"mode and falls back to the 32-bit kernel otherwise.\n";

static const char *notes =
"Terminal cheat sheet\n"
"  ls, cat, write, append, rm, cp, mv, touch, edit\n"
"  calc (2+3)*7     cpu     mem     memmap     aquafetch\n"
"  open paint       theme 1 2       beep 440 300\n";

static const char *todo =
"Ideas to try:\n"
" 1. Draw something in Paint\n"
" 2. Beat your Snake high score\n"
" 3. Change the wallpaper in Settings\n"
" 4. Write a note and save it with Ctrl+S\n";

/* ------------------------------------------------------------------ kmain */
void kmain(void) {
    gfx_init();
    hw_init();
    input_init();
    input_ready = 1;
    cpu_detect();
    memset32(g_back, RGB(6, 10, 16), (size_t)SW * SH);
    gfx_blit_all();
    bx = 8; by = 8;

    static const char *banner[] = {
        "    _                         ___  ____  ",
        "   / \\   __ _ _   _  __ _    / _ \\/ ___| ",
        "  / _ \\ / _` | | | |/ _` |  | | | \\___ \\ ",
        " / ___ \\ (_| | |_| | (_| |  | |_| |___) |",
        "/_/   \\_\\__, |\\__,_|\\__,_|   \\___/|____/ ",
        "           |_|                           ",
    };
    for (int i = 0; i < 6; i++) { con_puts(banner[i], COL_CYAN); con_nl(); }
    con_printf(COL_TXT, "AquaOS %s  (%d-bit kernel, built %s %s)\n", AQUA_VERSION, ARCH_BITS, __DATE__, __TIME__);
    con_puts("Press any key to speed up the boot.\n\n", COL_DIM);
    bdelay(700);

    klog(COL_TXT, "AquaOS kernel starting in %s mode", ARCH_BITS == 64 ? "64-bit long" : "32-bit protected");
    kok("Boot info block at 0x600 (magic %s)", BI->magic == 0x41555141 ? "AQUA" : "????");
    bdelay(250);
    klog(COL_TXT, "Boot loader: drive 0x%02x, CPU long mode %s -> %d-bit kernel selected",
         BI->boot_drive, BI->cpu_lm ? "available" : "not available", (int)BI->arch);
    bdelay(300);
    kok("Video: %dx%d x32, LFB at 0x%08x", SW, SH, BI->fb_addr);
    bdelay(250);
    kok("Console font: VGA ROM 8x16 copied (4096 bytes)");
    bdelay(250);

    /* ---- memory ---- */
    klog(COL_CYAN, "Memory map from BIOS (INT 15h, E820):");
    E820Entry *e = (E820Entry *)(uintptr_t)BI->e820_addr;
    static const char *tn[] = { "?", "usable", "reserved", "ACPI reclaimable", "ACPI NVS", "bad memory" };
    for (u32 i = 0; i < BI->e820_count; i++) {
        u32 lo = e[i].base_lo + e[i].len_lo, hi = e[i].base_hi + e[i].len_hi + (lo < e[i].base_lo);
        if (lo == 0) hi--; lo--;
        stamp(); con_printf(COL_TXT, "  BIOS-e820: %08x%08x - %08x%08x  %s", e[i].base_hi, e[i].base_lo, hi, lo,
                            e[i].type < 6 ? tn[e[i].type] : "?");
        con_nl();
        bdelay(130);
    }
    u32 kb = mem_total_kb();
    kok("%u MiB of usable RAM detected", kb / 1024);
    if (kb < 24 * 1024) klog(COL_WARN, "WARNING: less than 24 MiB RAM - AquaOS needs at least 32 MiB");
    bdelay(250);
    kok("Kernel at 0x00100000, %u KiB (code + buffers)", ((u32)(uintptr_t)__bss_end - 0x100000) / 1024);
    bdelay(250);
    if (ARCH_BITS == 64) { kok("Paging enabled: 4 GiB identity map (2 MiB pages), PAE + long mode active"); bdelay(300); }
    else { kok("Flat 4 GiB protected-mode segments, paging off"); bdelay(300); }

    /* ---- cpu ---- */
    klog(COL_CYAN, "Detecting processor:");
    if (g_cpu.has_cpuid) {
        klog(COL_TXT, "  vendor : %s", g_cpu.vendor); bdelay(150);
        klog(COL_TXT, "  model  : %s", g_cpu.brand); bdelay(150);
        klog(COL_TXT, "  family %u, model %u, stepping %u, max CPUID leaf %u", g_cpu.family, g_cpu.model, g_cpu.stepping, g_cpu.maxleaf); bdelay(150);
        char f[160]; f[0] = 0; int o = 0;
        struct { u32 bit; const char *n; } fl[] = { { 0, "fpu" }, { 4, "tsc" }, { 5, "msr" }, { 6, "pae" }, { 9, "apic" }, { 15, "cmov" }, { 23, "mmx" }, { 25, "sse" }, { 26, "sse2" } };
        for (unsigned i = 0; i < sizeof fl / sizeof fl[0]; i++) if (g_cpu.feat_edx & (1u << fl[i].bit)) o += ksnprintf(f + o, sizeof f - o, "%s ", fl[i].n);
        if (g_cpu.ext_edx & (1u << 29)) o += ksnprintf(f + o, sizeof f - o, "lm ");
        klog(COL_TXT, "  flags  : %s", f);
    } else klog(COL_WARN, "  no CPUID: very old processor, using generic x86 profile");
    kok("CPU initialised"); bdelay(250);

    /* ---- interrupts / timers ---- */
    kok("IDT loaded: 32 exception vectors + timer IRQ (vector 0x20)"); bdelay(250);
    kok("8259 PIC remapped to 0x20-0x2F, all IRQs masked except IRQ0"); bdelay(250);
    kok("PIT channel 0 programmed to 1000 Hz, interrupts enabled"); bdelay(250);
    RTime t; rtc_read(&t);
    kok("CMOS RTC: %04d-%02d-%02d %02d:%02d:%02d", t.year, t.mon, t.day, t.hour, t.min, t.sec); bdelay(300);
    ksrand(g_ms * 7919u + t.sec * 104729u + t.min);

    /* ---- input ---- */
    kok("i8042 controller initialised (scancode set 1)"); bdelay(200);
    kok("PS/2 keyboard ready, US layout"); bdelay(200);
    if (mouse_present) kok("PS/2 mouse detected (device id %d), streaming mode", mouse_id);
    else kstat(" WARN ", COL_WARN, "No PS/2 mouse found - keyboard only (Alt+Tab, arrows)");
    bdelay(250);

    /* ---- filesystem ---- */
    fs_init();
    kok("RAM filesystem: %d slots x %d KiB", FS_MAX, FS_FILESZ / 1024); bdelay(250);
    static const struct { const char *n; const char **d; } files[] = { { "README.txt", &readme }, { "about.txt", &about }, { "notes.txt", &notes }, { "todo.txt", &todo } };
    for (int i = 0; i < 4; i++) {
        fs_write(files[i].n, *files[i].d, (int)strlen(*files[i].d));
        kok("  installed /%s (%d bytes)", files[i].n, (int)strlen(*files[i].d)); bdelay(200);
    }

    /* ---- resources ---- */
    klog(COL_CYAN, "Loading system resources:");
    apps_register();
    kok("  cursor sprite 12x19"); bdelay(220);
    kok("  icon set (%d vector icons)", g_napps); bdelay(220);
    kok("  wallpaper themes: Aqua Deep, Coral Sunset, Midnight"); bdelay(220);
    kok("  accent palettes: 4"); bdelay(220);
    for (int i = 0; i < g_napps; i++) { kok("  application '%s'", g_apps[i]->name); bdelay(280); }
    bar("Unpacking resources  ", 2200);
    bar("Starting services    ", 2200);
    kok("Window manager and compositor"); bdelay(250);
    klog(COL_TXT, "Rendering wallpaper (%dx%d)...", SW, SH);
    gfx_target(g_back, SW);
    mouse_set_bounds(SW, SH);
    wm_init();
    gfx_target(g_back, SW);
    kok("Desktop ready"); bdelay(400);
    klog(COL_OK, "Boot finished in %u.%03u s", g_ms / 1000, g_ms % 1000);
    con_puts("\n", COL_TXT);
    bdelay(900);
    con_printf(COL_CYAN, "Starting AquaOS desktop...\n");
    bdelay(1100);

    /* ---- desktop ---- */
    key_flush();
    notepad_open("README.txt");
    wm_inval_all();
    int shown_x = -100, shown_y = 0, prev_btn = 0;
    gfx_target(g_back, SW);
    for (;;) {
        input_poll();
        int k;
        while ((k = key_get())) wm_key(k);
        if (mouse_moved) { mouse_moved = 0; wm_mouse(mouse_x, mouse_y, mouse_btn, prev_btn); prev_btn = mouse_btn; }
        wm_tick();
        int moved = (mouse_x != shown_x || mouse_y != shown_y);
        if (wm_compose_pending()) {
            if (shown_x >= 0) cursor_erase(shown_x, shown_y);
            wm_compose();
            cursor_draw(mouse_x, mouse_y);
            shown_x = mouse_x; shown_y = mouse_y;
        } else if (moved) {
            if (shown_x >= 0) cursor_erase(shown_x, shown_y);
            cursor_draw(mouse_x, mouse_y);
            shown_x = mouse_x; shown_y = mouse_y;
        }
        __asm__ volatile("hlt");
    }
}
