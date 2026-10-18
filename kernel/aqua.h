/* AquaOS - shared kernel header */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef uint64_t u64;

#define AQUA_VERSION "1.0"
#define abs(x) ((x) < 0 ? -(x) : (x))
#ifdef __x86_64__
#define ARCH_BITS 64
#define ARCH_NAME "x86_64"
#else
#define ARCH_BITS 32
#define ARCH_NAME "i386"
#endif

/* ---- boot info block, filled in by boot/boot.asm at physical 0x600 ---- */
typedef struct {
    u32 magic, boot_drive, e820_count, e820_addr;
    u32 fb_addr, fb_pitch, fb_w, fb_h, fb_bpp;
    u32 font_addr, cpu_lm, arch, vbe_ok;
} BootInfo;
typedef struct { u32 base_lo, base_hi, len_lo, len_hi, type, attr; } E820Entry;
#define BI ((volatile BootInfo *)0x600)

extern char __bss_end[];

/* ---- port io ---- */
static inline void outb(u16 p, u8 v)  { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline u8   inb(u16 p)         { u8 v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline u16  inw(u16 p)         { u16 v; __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static inline void io_wait(void)      { outb(0x80, 0); }

/* ---- lib.c ---- */
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int   memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int   strcmp(const char *a, const char *b);
int   strncmp(const char *a, const char *b, size_t n);
void  scopy(char *dst, const char *src, int max);
int   ksnprintf(char *buf, int n, const char *fmt, ...);
int   kvsnprintf(char *buf, int n, const char *fmt, va_list ap);
int   katoi(const char *s);
void  memset32(u32 *d, u32 v, size_t n);
void  memcpy32(u32 *d, const u32 *s, size_t n);
u32   krand(void);
void  ksrand(u32 s);
int   isin(int deg);   /* result -1000..1000 */
int   icos(int deg);

/* ---- hw.c ---- */
enum { K_UP = 0x101, K_DOWN, K_LEFT, K_RIGHT, K_HOME, K_END, K_DEL, K_PGUP, K_PGDN, K_WIN, K_F4 };
typedef struct { int year, mon, day, hour, min, sec, wday; } RTime;
typedef struct {
    int has_cpuid; char vendor[13]; char brand[49];
    u32 family, model, stepping, feat_edx, feat_ecx, ext_edx, maxleaf;
} CpuInfo;
extern volatile u32 g_ms;
extern CpuInfo g_cpu;
extern int mouse_present, mouse_id;
extern int mouse_x, mouse_y, mouse_btn, mouse_moved;
extern int kmod_shift, kmod_ctrl, kmod_alt;
void hw_init(void);          /* IDT + PIC + PIT, enables interrupts */
void cpu_detect(void);
void input_init(void);
void input_poll(void);
int  key_get(void);
void key_flush(void);
void delay_ms(u32 ms);
void rtc_read(RTime *t);
void speaker(u32 freq, u32 ms);
void sys_reboot(void);
void sys_shutdown(void);
u32  mem_total_kb(void);
void mouse_set_bounds(int w, int h);

/* ---- gfx.c ---- */
#define RGB(r, g, b) ((u32)(((r) << 16) | ((g) << 8) | (b)))
#define MAXW 1024
#define MAXH 768
typedef struct { int x, y, w, h; } Rect;
extern int SW, SH;
extern u32 *g_fb;
extern int g_fb_pitch;          /* pixels */
extern u32 g_back[MAXW * MAXH];
extern u32 g_wall[MAXW * MAXH];
void gfx_init(void);
void gfx_target(u32 *buf, int pitch);
Rect gfx_getclip(void);
void gfx_setclip_r(Rect r);
void gfx_setclip(int x, int y, int w, int h);
void gfx_clipreset(void);
Rect rect_isect(Rect a, Rect b);
Rect rect_union(Rect a, Rect b);
int  rect_hit(Rect r, int x, int y);
u32  mix(u32 a, u32 b, int t256);
void fill_rect(int x, int y, int w, int h, u32 c);
void fill_rect_alpha(int x, int y, int w, int h, u32 c, int a);
void fill_rrect(int x, int y, int w, int h, int r, u32 c);
void rect_outline(int x, int y, int w, int h, u32 c);
void hline(int x, int y, int w, u32 c);
void vline(int x, int y, int h, u32 c);
void line(int x0, int y0, int x1, int y1, u32 c);
void fill_circle(int cx, int cy, int r, u32 c);
void circle(int cx, int cy, int r, u32 c);
void gradient_v(int x, int y, int w, int h, u32 c1, u32 c2);
void draw_char(int x, int y, int ch, u32 fg);
void draw_text(int x, int y, const char *s, u32 fg);
void draw_text_s(int x, int y, const char *s, u32 fg, int scale);
int  text_w(const char *s);
void gfx_blit(int x, int y, int w, int h);   /* back buffer -> screen */
void gfx_blit_all(void);
void cursor_draw(int x, int y);
void cursor_erase(int x, int y);
void wall_render(int theme);

/* ---- fs.c ---- */
#define FS_MAX 48
#define FS_FILESZ 16384
typedef struct { char name[28]; int used; int size; char *data; } FsFile;
extern FsFile g_fs[FS_MAX];
void    fs_init(void);
FsFile *fs_find(const char *name);
FsFile *fs_create(const char *name);
int     fs_write(const char *name, const char *data, int len);
int     fs_append(const char *name, const char *data, int len);
int     fs_delete(const char *name);
int     fs_count(void);
int     fs_total_bytes(void);

/* ---- wm.c ---- */
enum { EV_DOWN = 1, EV_UP, EV_MOVE, EV_DBL };
typedef struct Window Window;
typedef struct App {
    const char *name, *title;
    int icon, w, h;
    void (*init)(Window *);
    void (*draw)(Window *, int cx, int cy, int cw, int ch);
    void (*mouse)(Window *, int mx, int my, int btn, int ev);
    void (*key)(Window *, int key);
    void (*tick)(Window *);
    void (*closed)(Window *);
} App;
struct Window { const App *app; int x, y, w, h; int sx, sy, sw, sh; int open, min, max; };
#define TITLE_H 26
#define TASK_H 40
extern const App *g_apps[];
extern int g_napps;
extern int g_theme, g_accent, g_show_secs;
void wm_init(void);
void wm_inval(int x, int y, int w, int h);
void wm_inval_all(void);
void wm_inval_win(Window *w);
void wm_compose(void);
void wm_mouse(int x, int y, int btn, int prev);
void wm_key(int key);
void wm_tick(void);
Window *wm_open(int idx);
void wm_close(Window *w);
Window *wm_win(int idx);
void win_content(Window *w, int *x, int *y, int *cw, int *ch);
void draw_icon(int id, int x, int y, int s);
void ui_button(int x, int y, int w, int h, const char *label, int pressed);
extern u32 g_acc_hi, g_acc_lo;
void wm_set_theme(int theme, int accent);
void wm_reboot_prompt(int shutdown);

/* ---- apps.c ---- */
void apps_register(void);
void notepad_open(const char *name);
