#include "uikit.h"

static void tree(const char *p, u32 *items, u32 *bytes, int depth) {
    GDirEnt de;
    for (int i = 0; depth < 6 && gos_readdir(p, i, &de) > 0; i++) {
        char q[160]; ksnprintf(q, sizeof q, "%s/%s", !strcmp(p, "/") ? "" : p, de.name); GStat st;
        (*items)++;
        if (de.type == VN_DIR) tree(q, items, bytes, depth + 1); else if (gos_stat(q, &st) == 0) *bytes += st.size;
    }
}

static const char *kind(const char *name, int type) {
    const char *e = strrchr(name, '.');
    if (type == VN_DIR) return "folder";
    if (e && !strcmp(e, ".app")) return "application (shortcut)";
    if (e && !strcmp(e, ".sh")) return "shell script";
    if (e && (!strcmp(e, ".txt") || !strcmp(e, ".md"))) return "text document";
    if (e && (!strcmp(e, ".c") || !strcmp(e, ".h"))) return "source code";
    return "document";
}

int main(int argc, char **argv) {
    static Win w; char path[128], name[40], where[128]; GStat st; GEvent e; int quit = 0, dirty = 1, locked;
    strlcpy(path, argc > 1 ? argv[1] : "/", sizeof path);
    if (gos_stat(path, &st) < 0) { win_msgbox("Get Info", "Item not found.", path); return 1; }
    const char *sl = strrchr(path, '/');
    strlcpy(name, !strcmp(path, "/") ? "GOS HD" : sl ? sl + 1 : path, sizeof name);
    strlcpy(where, path, sizeof where);
    { char *s = strrchr(where, '/'); if (s) { if (s == where) where[1] = 0; else *s = 0; } }
    u32 items = 0, bytes = st.size;
    if (st.type == VN_DIR) { bytes = 0; tree(path, &items, &bytes, 0); }
    locked = !(st.mode & 0222);
    char title[48]; ksnprintf(title, sizeof title, "%s Info", name);
    if (win_create(&w, title, -1, 0, 330, 270) < 0) return 1;
    win_set_minsize(&w, 330, 270);
    while (!quit) {
        if (dirty) {
            win_begin(&w); draw_rect(0, 0, w.cw, w.ch, C_WIN);
            icon_draw(10, 8, !strcmp(path, "/") ? ICON_DRIVE : !strcmp(path, GOS_TRASH_DIR) ? ICON_TRASH : st.type == VN_DIR ? ICON_FOLDER : ICON_FILE);
            ftext_draw(FACE_CHICAGO, 54, 16, name, C_BLK, -1);
            draw_rect(8, 46, w.cw - 16, 1, C_BLK);
            char b[96]; DateTime t; epoch_to_dt(st.mtime, &t); int y = 54;
            #define ROW(lbl, ...) do { ftext_draw(FACE_GENEVA, 10, y, lbl, C_DGR, -1); ksnprintf(b, sizeof b, __VA_ARGS__); ftext_draw(FACE_GENEVA, 86, y, b, C_BLK, -1); y += 20; } while (0)
            ROW("Kind:", "%s", !strcmp(path, "/") ? "disk" : !strcmp(path, GOS_TRASH_DIR) ? "Trash" : kind(name, st.type));
            if (st.type == VN_DIR) ROW("Size:", "%u item, %u K", items, (bytes + 1023) / 1024);
            else ROW("Size:", "%u K (%u bytes)", (bytes + 1023) / 1024, bytes);
            ROW("Where:", "%s", where);
            ROW("Modified:", "%02d/%02d/%d %02d:%02d", t.day, t.mon, t.year, t.hour, t.min);
            ROW("Owner:", "%s (uid %d)", st.uid == 0 ? "root" : "user", st.uid);
            ROW("Mode:", "%04o", st.mode & 07777);
            draw_rect(8, y + 2, w.cw - 16, 1, C_BLK);
            ui_check(12, y + 12, "Locked (read-only)", locked);
            win_set_info(&w, locked ? "Item is locked" : "Item is unlocked");
            win_present(&w); dirty = 0;
        }
        if (win_next(&w, &e, -1) <= 0) break;
        do {
            if (e.type == GE_MOUSE_DOWN && e.b >= 190 && e.b < 224 && e.a < 220) {
                int nm = locked ? (st.mode | 0200) : (st.mode & ~0222);
                if (gos_chmod(path, nm) < 0) win_msgbox("Get Info", "Can't change this item's permissions.", "");
                else { st.mode = (u16)nm; locked = !locked; }
                dirty = 1;
            } else if (e.type == GE_RESIZE) dirty = 1;
            else if (e.type == GE_CLOSE || (e.type == GE_KEY && e.a == 27)) quit = 1;
        } while (!quit && win_next(&w, &e, 0) > 0);
    }
    win_close(&w);
    return 0;
}
