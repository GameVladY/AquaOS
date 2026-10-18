/* AquaOS - hardware layer: IDT, PIC, PIT, RTC, CPUID, PS/2 keyboard+mouse, power */
#include "aqua.h"

volatile u32 g_ms;
CpuInfo g_cpu;
int mouse_present, mouse_id;
int mouse_x = 400, mouse_y = 300, mouse_btn, mouse_moved;
int kmod_shift, kmod_ctrl, kmod_alt;
static int mb_w = 800, mb_h = 600;

/* ================= interrupts ================= */
extern void (*isr_table[32])(void);
extern void irq0_stub(void);
extern void irq_ignore(void);

#ifdef __x86_64__
typedef struct { u16 lo; u16 sel; u8 ist; u8 attr; u16 mid; u32 hi; u32 zero; } __attribute__((packed)) IdtGate;
typedef struct { u16 limit; u64 base; } __attribute__((packed)) Idtr;
#define KCODE 0x18
#else
typedef struct { u16 lo; u16 sel; u8 zero; u8 attr; u16 hi; } __attribute__((packed)) IdtGate;
typedef struct { u16 limit; u32 base; } __attribute__((packed)) Idtr;
#define KCODE 0x08
#endif
static IdtGate idt[48];

static void set_gate(int n, void (*fn)(void)) {
    uintptr_t a = (uintptr_t)fn;
    idt[n].lo = a & 0xFFFF;
    idt[n].sel = KCODE;
    idt[n].attr = 0x8E;
#ifdef __x86_64__
    idt[n].ist = 0;
    idt[n].mid = (a >> 16) & 0xFFFF;
    idt[n].hi = (u32)(a >> 32);
    idt[n].zero = 0;
#else
    idt[n].zero = 0;
    idt[n].hi = (a >> 16) & 0xFFFF;
#endif
}

void timer_tick(void) {
    g_ms++;
    outb(0x20, 0x20);
}

static const char *exc_names[32] = {
    "Divide error", "Debug", "NMI", "Breakpoint", "Overflow", "Bound range", "Invalid opcode",
    "Device not available", "Double fault", "Coprocessor overrun", "Invalid TSS", "Segment not present",
    "Stack fault", "General protection fault", "Page fault", "Reserved", "x87 FPU error", "Alignment check",
    "Machine check", "SIMD error", "Virtualization", "Control protection", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Hypervisor", "VMM comm", "Security", "Reserved"
};

void exception_handler(uintptr_t *frame) {
    char b[96];
    __asm__ volatile("cli");
    gfx_target(g_fb, g_fb_pitch);
    gfx_clipreset();
    fill_rect(0, 0, SW, SH, RGB(0, 40, 110));
    draw_text_s(40, 40, ":(  AquaOS has stopped", RGB(255, 255, 255), 3);
    draw_text(40, 120, "A fatal CPU exception occurred and the kernel cannot continue.", RGB(220, 235, 255));
    ksnprintf(b, sizeof b, "Exception %d: %s", (int)frame[0], frame[0] < 32 ? exc_names[frame[0]] : "?");
    draw_text(40, 160, b, RGB(255, 255, 160));
    ksnprintf(b, sizeof b, "Error code: 0x%x    Instruction pointer: 0x%x", (u32)frame[1], (u32)frame[2]);
    draw_text(40, 184, b, RGB(255, 255, 160));
    ksnprintf(b, sizeof b, "Build: AquaOS %s (%s)", AQUA_VERSION, ARCH_NAME);
    draw_text(40, 208, b, RGB(180, 200, 230));
    draw_text(40, 260, "Press any key to restart the computer.", RGB(255, 255, 255));
    key_flush();
    for (;;) { input_poll(); if (key_get()) sys_reboot(); }
}

void hw_init(void) {
    for (int i = 0; i < 32; i++) set_gate(i, isr_table[i]);
    for (int i = 32; i < 48; i++) set_gate(i, irq_ignore);
    set_gate(0x20, irq0_stub);
    Idtr r = { sizeof idt - 1, (uintptr_t)idt };
    __asm__ volatile("lidt %0" :: "m"(r));
    /* remap PIC: IRQs -> 0x20..0x2F, only IRQ0 (timer) unmasked */
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();
    outb(0x21, 0xFE); outb(0xA1, 0xFF);
    /* PIT channel 0: rate generator, ~1000 Hz */
    outb(0x43, 0x34);
    outb(0x40, 1193 & 0xFF);
    outb(0x40, 1193 >> 8);
    __asm__ volatile("sti");
}

void delay_ms(u32 ms) {
    u32 t = g_ms;
    while (g_ms - t < ms) __asm__ volatile("hlt");
}

/* ================= RTC ================= */
static int rtc_reg(int r) { outb(0x70, r); return inb(0x71); }
static int bcd(int v) { return (v & 15) + (v >> 4) * 10; }
void rtc_read(RTime *t) {
    int tries = 0;
    while ((rtc_reg(0x0A) & 0x80) && tries++ < 10000) ;
    int s = rtc_reg(0), m = rtc_reg(2), h = rtc_reg(4), d = rtc_reg(7), mo = rtc_reg(8), y = rtc_reg(9);
    int rb = rtc_reg(0x0B);
    int pm = h & 0x80; h &= 0x7F;
    if (!(rb & 4)) { s = bcd(s); m = bcd(m); h = bcd(h); d = bcd(d); mo = bcd(mo); y = bcd(y); }
    if (!(rb & 2)) { if (h == 12) h = 0; if (pm) h += 12; }
    t->sec = s; t->min = m; t->hour = h; t->day = d ? d : 1; t->mon = mo ? mo : 1; t->year = 2000 + y;
    /* day of week, Sakamoto */
    static const int tt[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int yy = t->year; if (t->mon < 3) yy--;
    t->wday = (yy + yy / 4 - yy / 100 + yy / 400 + tt[(t->mon - 1) % 12] + t->day) % 7;
}

/* ================= CPUID ================= */
static inline void cpuid(u32 leaf, u32 *a, u32 *b, u32 *c, u32 *d) {
    __asm__ volatile("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d) : "a"(leaf), "c"(0));
}
static int cpuid_available(void) {
    u32 f1, f2;
#ifdef __x86_64__
    __asm__ volatile("pushfq; pop %%rax; mov %%eax,%0; xor $0x200000,%%eax; push %%rax; popfq;"
                     "pushfq; pop %%rax; mov %%eax,%1" : "=r"(f1), "=r"(f2) :: "rax", "cc");
#else
    __asm__ volatile("pushfl; pop %%eax; mov %%eax,%0; xor $0x200000,%%eax; push %%eax; popfl;"
                     "pushfl; pop %%eax; mov %%eax,%1" : "=r"(f1), "=r"(f2) :: "eax", "cc");
#endif
    return (f1 ^ f2) & 0x200000;
}
void cpu_detect(void) {
    memset(&g_cpu, 0, sizeof g_cpu);
    scopy(g_cpu.vendor, "unknown", 13);
    scopy(g_cpu.brand, "Generic x86 CPU", 49);
    if (!cpuid_available()) return;
    g_cpu.has_cpuid = 1;
    u32 a, b, c, d;
    cpuid(0, &a, &b, &c, &d);
    g_cpu.maxleaf = a;
    memcpy(g_cpu.vendor, &b, 4); memcpy(g_cpu.vendor + 4, &d, 4); memcpy(g_cpu.vendor + 8, &c, 4);
    g_cpu.vendor[12] = 0;
    if (a >= 1) {
        cpuid(1, &a, &b, &c, &d);
        g_cpu.stepping = a & 15; g_cpu.model = (a >> 4) & 15; g_cpu.family = (a >> 8) & 15;
        if (g_cpu.family == 15) g_cpu.family += (a >> 20) & 255;
        if (g_cpu.family >= 6) g_cpu.model += ((a >> 16) & 15) << 4;
        g_cpu.feat_ecx = c; g_cpu.feat_edx = d;
    }
    cpuid(0x80000000, &a, &b, &c, &d);
    if (a >= 0x80000001) { cpuid(0x80000001, &a, &b, &c, &d); g_cpu.ext_edx = d; }
    cpuid(0x80000000, &a, &b, &c, &d);
    if (a >= 0x80000004) {
        u32 *p = (u32 *)g_cpu.brand;
        for (u32 i = 0; i < 3; i++) { cpuid(0x80000002 + i, &p[i * 4], &p[i * 4 + 1], &p[i * 4 + 2], &p[i * 4 + 3]); }
        g_cpu.brand[48] = 0;
        char *s = g_cpu.brand; while (*s == ' ') s++;
        if (s != g_cpu.brand) memmove(g_cpu.brand, s, strlen(s) + 1);
    }
}

u32 mem_total_kb(void) {
    E820Entry *e = (E820Entry *)(uintptr_t)BI->e820_addr;
    u32 kb = 0;
    for (u32 i = 0; i < BI->e820_count; i++)
        if (e[i].type == 1) kb += (e[i].len_hi << 22) + (e[i].len_lo >> 10);
    return kb ? kb : 32768;
}

/* ================= PS/2 keyboard + mouse ================= */
static int kq[64], kq_h, kq_t;
static void kpush(int k) { int n = (kq_t + 1) & 63; if (n != kq_h) { kq[kq_t] = k; kq_t = n; } }
int key_get(void) { if (kq_h == kq_t) return 0; int k = kq[kq_h]; kq_h = (kq_h + 1) & 63; return k; }
void key_flush(void) { kq_h = kq_t = 0; }

static const char sc_norm[] = "\0\033" "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char sc_shft[] = "\0\033" "!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
static int caps, e0;

static void kbd_byte(u8 d) {
    if (d == 0xE0) { e0 = 1; return; }
    if (d == 0xE1 || d == 0xFA || d == 0xFE) { return; }
    int rel = d & 0x80, sc = d & 0x7F, ext = e0; e0 = 0;
    if (!ext && (sc == 0x2A || sc == 0x36)) { kmod_shift = !rel; return; }
    if (sc == 0x1D) { kmod_ctrl = !rel; return; }
    if (sc == 0x38) { kmod_alt = !rel; return; }
    if (rel) return;
    if (!ext && sc == 0x3A) { caps = !caps; return; }
    int k = 0;
    if (ext) {
        switch (sc) {
        case 0x48: k = K_UP; break;    case 0x50: k = K_DOWN; break;
        case 0x4B: k = K_LEFT; break;  case 0x4D: k = K_RIGHT; break;
        case 0x47: k = K_HOME; break;  case 0x4F: k = K_END; break;
        case 0x53: k = K_DEL; break;   case 0x49: k = K_PGUP; break;
        case 0x51: k = K_PGDN; break;  case 0x5B: case 0x5C: k = K_WIN; break;
        case 0x1C: k = '\n'; break;    case 0x35: k = '/'; break;
        }
    } else if (sc == 0x3E) k = K_F4;
    else if (sc < sizeof(sc_norm) - 1) {
        char c = kmod_shift ? sc_shft[sc] : sc_norm[sc];
        if (caps && c >= 'a' && c <= 'z' && !kmod_shift) c -= 32;
        else if (caps && c >= 'A' && c <= 'Z' && kmod_shift) c += 32;
        k = (u8)c;
    }
    if (k) kpush(k);
}

static u8 mpkt[3]; static int midx;
static void mouse_byte(u8 d) {
    if (midx == 0 && !(d & 8)) return;           /* resync */
    mpkt[midx++] = d;
    if (midx < 3) return;
    midx = 0;
    int fl = mpkt[0];
    int dx = mpkt[1] - ((fl << 4) & 0x100);
    int dy = mpkt[2] - ((fl << 3) & 0x100);
    if (fl & 0xC0) return;                        /* overflow */
    mouse_x += dx; mouse_y -= dy;
    if (mouse_x < 0) mouse_x = 0; if (mouse_x >= mb_w) mouse_x = mb_w - 1;
    if (mouse_y < 0) mouse_y = 0; if (mouse_y >= mb_h) mouse_y = mb_h - 1;
    mouse_btn = fl & 7;
    mouse_moved = 1;
}
void mouse_set_bounds(int w, int h) { mb_w = w; mb_h = h; mouse_x = w / 2; mouse_y = h / 2; }

void input_poll(void) {
    for (int n = 0; n < 64; n++) {
        u8 st = inb(0x64);
        if (!(st & 1)) break;
        u8 d = inb(0x60);
        if (st & 0x20) mouse_byte(d); else kbd_byte(d);
    }
}

static int kbc_wait_in(void)  { for (int i = 0; i < 200000; i++) if (!(inb(0x64) & 2)) return 1; return 0; }
static int kbc_wait_out(void) { for (int i = 0; i < 200000; i++) if (inb(0x64) & 1) return 1; return 0; }
static void kbc_cmd(u8 c)  { kbc_wait_in(); outb(0x64, c); }
static void kbc_data(u8 d) { kbc_wait_in(); outb(0x60, d); }
static int  dev_read(void) { return kbc_wait_out() ? inb(0x60) : -1; }
static void mouse_write(u8 b) { kbc_cmd(0xD4); kbc_data(b); }
static int mouse_cmd(u8 b) {
    mouse_write(b);
    for (int i = 0; i < 4; i++) { int r = dev_read(); if (r == 0xFA) return 1; if (r < 0) break; }
    return 0;
}

void input_init(void) {
    while (inb(0x64) & 1) inb(0x60);              /* flush */
    kbc_cmd(0xA8);                                /* enable aux port */
    kbc_cmd(0x20);
    int cfg = dev_read(); if (cfg < 0) cfg = 0x45;
    cfg &= ~0x33;                                 /* no IRQs, clocks on */
    cfg |= 0x40;                                  /* scancode set 1 translation */
    kbc_cmd(0x60); kbc_data(cfg);
    kbc_data(0xF4); dev_read();                   /* keyboard: enable scanning */
    mouse_present = 0;
    if (mouse_cmd(0xF6)) {                        /* defaults */
        mouse_write(0xF2);
        int a = dev_read(), id = dev_read();
        mouse_id = (a == 0xFA && id >= 0) ? id : 0;
        if (mouse_cmd(0xF4)) mouse_present = 1;
    }
    midx = 0; key_flush();
}

/* ================= speaker & power ================= */
void speaker(u32 freq, u32 ms) {
    if (freq < 20) return;
    u32 div = 1193182 / freq;
    outb(0x43, 0xB6); outb(0x42, div & 0xFF); outb(0x42, div >> 8);
    outb(0x61, inb(0x61) | 3);
    delay_ms(ms);
    outb(0x61, inb(0x61) & ~3);
}

void sys_reboot(void) {
    __asm__ volatile("cli");
    for (int i = 0; i < 100000; i++) { if (!(inb(0x64) & 2)) break; }
    outb(0x64, 0xFE);                             /* pulse CPU reset line */
    for (volatile int i = 0; i < 1000000; i++) ;
    Idtr z = { 0, 0 };                            /* fallback: triple fault */
    __asm__ volatile("lidt %0; int3" :: "m"(z));
    for (;;) __asm__ volatile("hlt");
}

void sys_shutdown(void) {
    __asm__ volatile("cli");
    outw(0x604, 0x2000);                          /* QEMU (new) */
    outw(0xB004, 0x2000);                         /* Bochs / old QEMU */
    outw(0x4004, 0x3400);                         /* VirtualBox */
    outw(0x600, 0x34);
    for (;;) __asm__ volatile("hlt");
}
