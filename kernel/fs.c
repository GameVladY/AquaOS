/* AquaOS - RAM filesystem (flat namespace, contents live in memory until reboot) */
#include "aqua.h"

FsFile g_fs[FS_MAX];
static char pool[FS_MAX][FS_FILESZ];

void fs_init(void) {
    memset(g_fs, 0, sizeof g_fs);
    for (int i = 0; i < FS_MAX; i++) g_fs[i].data = pool[i];
}
FsFile *fs_find(const char *name) {
    for (int i = 0; i < FS_MAX; i++) if (g_fs[i].used && !strcmp(g_fs[i].name, name)) return &g_fs[i];
    return 0;
}
FsFile *fs_create(const char *name) {
    if (!name[0] || strlen(name) >= sizeof g_fs[0].name) return 0;
    FsFile *f = fs_find(name);
    if (f) return f;
    for (int i = 0; i < FS_MAX; i++)
        if (!g_fs[i].used) {
            g_fs[i].used = 1; g_fs[i].size = 0;
            scopy(g_fs[i].name, name, sizeof g_fs[i].name);
            return &g_fs[i];
        }
    return 0;
}
int fs_write(const char *name, const char *data, int len) {
    FsFile *f = fs_create(name);
    if (!f) return -1;
    if (len > FS_FILESZ) len = FS_FILESZ;
    memcpy(f->data, data, len);
    f->size = len;
    return len;
}
int fs_append(const char *name, const char *data, int len) {
    FsFile *f = fs_create(name);
    if (!f) return -1;
    if (f->size + len > FS_FILESZ) len = FS_FILESZ - f->size;
    memcpy(f->data + f->size, data, len);
    f->size += len;
    return len;
}
int fs_delete(const char *name) {
    FsFile *f = fs_find(name);
    if (!f) return -1;
    f->used = 0; f->size = 0; f->name[0] = 0;
    return 0;
}
int fs_count(void) { int n = 0; for (int i = 0; i < FS_MAX; i++) n += g_fs[i].used; return n; }
int fs_total_bytes(void) { int n = 0; for (int i = 0; i < FS_MAX; i++) if (g_fs[i].used) n += g_fs[i].size; return n; }
