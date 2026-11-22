#include "gos.h"

#define NHIST 60
typedef struct { u8 h[NHIST]; int sel_pid; int sc_pos, sc_total, sc_view; int dirty, quit; Win *w; GSysInfo si; } Mon;

static void bar(int x, int y, int w, int pct, u8 col) {
    draw_box(x, y, w, 12, C_WHT, C_BLK);
    draw_rect(x + 1, y + 1, (w - 2) * pct / 100, 10, col);
}

static void mon_draw(Mon *m) {
    Win *w = m->w;
    win_begin(w);
    int cw = w->cw, ch = w->ch; char b[96];
    GSysInfo *si = &m->si;
    draw_rect(0, 0, cw, ch, C_WIN);
    int gw = cw - 16 < NHIST * 3 ? cw - 16 : NHIST * 3;
    ksnprintf(b, sizeof b, "CPU %u%%", si->cpu_load); ftext_draw(FACE_GENEVA, 8, 6, b, C_BLK, -1);
    draw_box(8, 24, gw, 50, C_BLK, C_BLK);
    for (int i = 0; i < NHIST && i * 3 < gw - 2; i++) {
        int v = m->h[NHIST - 1 - i] * 48 / 100;
        draw_rect(8 + gw - 3 - i * 3, 24 + 49 - v, 3, v, C_GRN);
    }
    u32 hu = si->heap_used / 1024, ht = si->heap_total / 1024;
    ksnprintf(b, sizeof b, "Heap kernel %u / %u KB", hu, ht); ftext_draw(FACE_GENEVA, 8, 82, b, C_BLK, -1);
    bar(8, 98, gw, ht ? hu * 100 / ht : 0, C_BLU);
    u32 used = si->pmm_total - si->pmm_free;
    ksnprintf(b, sizeof b, "Frame PMM %u / %u KB", used * 4, si->pmm_total * 4); ftext_draw(FACE_GENEVA, 8, 114, b, C_BLK, -1);
    bar(8, 130, gw, si->pmm_total ? used * 100 / si->pmm_total : 0, C_GRN);
    u32 s = si->hz ? si->ticks / si->hz : 0;
    ksnprintf(b, sizeof b, "RAM %u MB  Uptime %u:%02u:%02u  Disk %s  Windows %u", si->ram_kb / 1024, s / 3600, s / 60 % 60, s % 60, si->ata ? "ATA" : "-", si->nwin);
    ftext_draw(FACE_GENEVA, 8, 148, b, C_BLK, -1);
    draw_rect(0, 168, cw, 1, C_BLK);
    static const struct { int x; const char *t; } hd[] = { {8,"PID"}, {48,"NAME"}, {176,"RING"}, {224,"STATE"}, {280,"CPU"}, {336,"MEM"} };
    for (int i = 0; i < 6; i++) ftext_draw(FACE_GENEVA | FS_BOLD, hd[i].x, 172, hd[i].t, C_BLK, -1);

    GProcInfo pi[GOS_MAXTASK]; int n = 0;
    for (int i = 0; i < GOS_MAXTASK; i++) if (gos_procinfo(i, &pi[n]) == 0) n++;
    int top = 190, view = (ch - top) / 16; if (view < 1) view = 1;
    m->sc_total = n; m->sc_view = view;
    int mx = n - view; if (mx < 0) mx = 0;
    if (m->sc_pos > mx) m->sc_pos = mx;
    if (m->sc_pos < 0) m->sc_pos = 0;
    static const char *st[] = { "-", "ready", "run", "sleep", "zombie" };
    for (int i = m->sc_pos; i < n; i++) {
        int ry = top + (i - m->sc_pos) * 16;
        if (ry > ch) break;
        if (pi[i].pid == m->sel_pid) draw_rect(2, ry, cw - 4, 16, C_BLK);
        u8 fg = pi[i].pid == m->sel_pid ? C_WHT : C_BLK;
        static const int cx[6] = { 8, 48, 176, 224, 280, 336 };
        char nm[40]; ftext_fit(FACE_GENEVA, pi[i].name, cx[2] - cx[1] - 8, nm, sizeof nm);
        const char *cell[6]; char c0[12], c4[12], c5[16];
        ksnprintf(c0, sizeof c0, "%d", pi[i].pid); ksnprintf(c4, sizeof c4, "%u", pi[i].cpu); ksnprintf(c5, sizeof c5, "%uK", pi[i].mem_kb);
        cell[0] = c0; cell[1] = nm; cell[2] = pi[i].user ? "3" : "0"; cell[3] = st[pi[i].state > 4 ? 0 : pi[i].state]; cell[4] = c4; cell[5] = c5;
        for (int k = 0; k < 6; k++) ftext_draw(FACE_GENEVA, cx[k], ry, cell[k], fg, -1);
    }
    win_set_scroll(w, m->sc_pos, n, view);
    win_present(w);
}

static void mon_tick(Mon *m) {
    char b[80];
    gos_sysinfo(&m->si);
    memmove(m->h, m->h + 1, NHIST - 1); m->h[NHIST - 1] = (u8)m->si.cpu_load;
    ksnprintf(b, sizeof b, "CPU %u%%  heap %u/%u KB", m->si.cpu_load, m->si.heap_used / 1024, m->si.heap_total / 1024);
    win_set_info(m->w, b);
    m->dirty = 1;
}

static void mon_mouse(Mon *m, int ly) {
    m->dirty = 1;
    if (ly < 190) return;
    int i = (ly - 190) / 16 + m->sc_pos, row = 0;
    for (int k = 0; k < GOS_MAXTASK; k++) {
        GProcInfo pi; if (gos_procinfo(k, &pi) < 0) continue;
        if (row++ == i) { m->sel_pid = pi.pid; return; }
    }
    m->sel_pid = -1;
}

int main(void) {
    static Win w; static Mon m;
    if (win_create(&w, "System Monitor", -1, 0, 430, 340) < 0) return 1;
    win_set_minsize(&w, 300, 220);
    m.w = &w; m.sel_pid = -1;
    mon_tick(&m);

    GEvent e;
    while (!m.quit) {
        if (m.dirty) { mon_draw(&m); m.dirty = 0; }
        int r = win_next(&w, &e, 1000);
        if (r < 0) break;
        if (r == 0) { mon_tick(&m); continue; }
        do {
            switch (e.type) {
            case GE_MOUSE_DOWN: mon_mouse(&m, e.b); break;
            case GE_KEY:
                if (e.a == KEY_DEL && m.sel_pid > 0) { gos_kill(m.sel_pid); m.sel_pid = -1; }
                m.dirty = 1; break;
            case GE_SCROLL: m.sc_pos = e.a; m.dirty = 1; break;
            case GE_RESIZE: m.dirty = 1; break;
            case GE_CLOSE:  m.quit = 1; break;
            default: break;
            }
        } while (!m.quit && (r = win_next(&w, &e, 0)) > 0);
        if (r < 0) break;
    }
    win_close(&w);
    return 0;
}
