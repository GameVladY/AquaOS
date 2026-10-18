/* AquaOS - tiny freestanding C library */
#include "aqua.h"

void *memcpy(void *d, const void *s, size_t n) {
    void *r = d;
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) :: "memory");
    return r;
}
void *memmove(void *d, const void *s, size_t n) {
    u8 *dd = d; const u8 *ss = s;
    if (dd == ss || n == 0) return d;
    if (dd < ss) { __asm__ volatile("rep movsb" : "+D"(dd), "+S"(ss), "+c"(n) :: "memory"); }
    else { dd += n; ss += n; while (n--) *--dd = *--ss; }
    return d;
}
void *memset(void *d, int c, size_t n) {
    void *r = d;
    __asm__ volatile("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return r;
}
int memcmp(const void *a, const void *b, size_t n) {
    const u8 *x = a, *y = b;
    while (n--) { if (*x != *y) return *x - *y; x++; y++; }
    return 0;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}
int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    return n ? (u8)*a - (u8)*b : 0;
}
void scopy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}
void memset32(u32 *d, u32 v, size_t n) {
    __asm__ volatile("rep stosl" : "+D"(d), "+c"(n) : "a"(v) : "memory");
}
void memcpy32(u32 *d, const u32 *s, size_t n) {
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(n) :: "memory");
}

int katoi(const char *s) {
    int neg = 0, v = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

/* ---- printf ---- */
int kvsnprintf(char *buf, int n, const char *fmt, va_list ap) {
    int o = 0;
#define PUT(ch) do { if (o < n - 1) buf[o] = (ch); o++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') { PUT(*fmt); continue; }
        fmt++;
        int left = 0, zero = 0, width = 0;
        if (*fmt == '-') { left = 1; fmt++; }
        if (*fmt == '0') { zero = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        if (*fmt == 'l') fmt++;
        char tmp[16]; int tl = 0; const char *str = 0;
        char c = *fmt;
        if (c == 'd' || c == 'i') {
            int v = va_arg(ap, int); u32 u = v < 0 ? -(u32)v : (u32)v;
            do { tmp[tl++] = '0' + u % 10; u /= 10; } while (u);
            if (v < 0) tmp[tl++] = '-';
        } else if (c == 'u') {
            u32 u = va_arg(ap, u32);
            do { tmp[tl++] = '0' + u % 10; u /= 10; } while (u);
        } else if (c == 'x' || c == 'X') {
            u32 u = va_arg(ap, u32);
            const char *dg = c == 'x' ? "0123456789abcdef" : "0123456789ABCDEF";
            do { tmp[tl++] = dg[u & 15]; u >>= 4; } while (u);
        } else if (c == 'c') {
            tmp[tl++] = (char)va_arg(ap, int);
        } else if (c == 's') {
            str = va_arg(ap, const char *); if (!str) str = "(null)";
        } else if (c == '%') {
            PUT('%'); continue;
        } else { PUT('%'); PUT(c); continue; }
        int len = str ? (int)strlen(str) : tl;
        int pad = width > len ? width - len : 0;
        if (!left) while (pad--) PUT(zero && !str ? '0' : ' ');
        if (str) { for (int i = 0; i < len; i++) PUT(str[i]); }
        else {
            /* put minus sign before zero padding */
            for (int i = tl - 1; i >= 0; i--) PUT(tmp[i]);
        }
        if (left) while (pad--) PUT(' ');
    }
    if (n > 0) buf[o < n ? o : n - 1] = 0;
    return o;
#undef PUT
}
int ksnprintf(char *buf, int n, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = kvsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

/* ---- random / trig ---- */
static u32 rng = 0x1234ABCD;
void ksrand(u32 s) { rng = s ? s : 0x1234ABCD; }
u32 krand(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* Bhaskara approximation, integer only. */
int isin(int deg) {
    deg %= 360; if (deg < 0) deg += 360;
    int neg = 0;
    if (deg >= 180) { deg -= 180; neg = 1; }
    int p = deg * (180 - deg);
    int v = 4 * p * 1000 / (40500 - p);
    return neg ? -v : v;
}
int icos(int deg) { return isin(deg + 90); }
