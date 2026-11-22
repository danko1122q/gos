#ifndef USER_PUZZLE_LOGIC_H
#define USER_PUZZLE_LOGIC_H
static inline void pz_solved_board(int *b) { for (int i = 0; i < 15; i++) b[i] = i + 1; b[15] = 0; }
static inline int pz_is_solved(const int *b) { for (int i = 0; i < 15; i++) if (b[i] != i + 1) return 0; return b[15] == 0; }
static inline int pz_blank(const int *b) { for (int i = 0; i < 16; i++) if (!b[i]) return i; return -1; }
static inline int pz_valid(const int *b) { int seen = 0; for (int i = 0; i < 16; i++) { if (b[i] < 0 || b[i] > 15 || (seen & (1 << b[i]))) return 0; seen |= 1 << b[i]; } return 1; }
static inline int pz_move(int *b, int i) {
    int z = pz_blank(b), r = i / 4, c = i % 4, zr = z / 4, zc = z % 4;
    if (i == z || ((r == zr) + (c == zc) == 0) || (r != zr && c != zc)) return 0;
    if ((r - zr) * (r - zr) + (c - zc) * (c - zc) != 1) return 0;
    b[z] = b[i]; b[i] = 0; return 1;
}
static inline void pz_shuffle(int *b, unsigned *st, int n) {
    pz_solved_board(b); int last = -1;
    for (int k = 0; k < n; k++) {
        *st = *st * 1103515245u + 12345u;
        int z = pz_blank(b), d = (int)((*st >> 16) % 4), t = -1;
        if (d == 0 && z >= 4) t = z - 4; else if (d == 1 && z < 12) t = z + 4;
        else if (d == 2 && z % 4) t = z - 1; else if (d == 3 && z % 4 != 3) t = z + 1;
        if (t < 0 || t == last) continue;
        last = z; pz_move(b, t);
    }
}
static inline int pz_solvable(const int *b) {
    int inv = 0; for (int i = 0; i < 16; i++) for (int j = i + 1; j < 16; j++) if (b[i] && b[j] && b[i] > b[j]) inv++;
    return ((inv + pz_blank(b) / 4) & 1) == (3 & 1) ? 1 : 0;
}
#endif
