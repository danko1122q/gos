#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

typedef uint32_t u32; typedef int32_t s32; typedef uint8_t u8; typedef uint16_t u16; typedef uint64_t u64; typedef int64_t s64;

#define RAM_SIZE (128u << 20)
static u8 *ram;
static int trace;

enum { M_USR = 0x10, M_FIQ = 0x11, M_IRQ = 0x12, M_SVC = 0x13, M_ABT = 0x17, M_UND = 0x1B, M_SYS = 0x1F };
static u32 r[16];
static u32 cpsr, spsr[6];
static struct { u32 sp, lr, r8_12[5]; } bank[6];
static u32 sctlr, ttbr, dacr, fsr, far_;
static int halted;
static u64 icount;

static int midx(u32 m) { switch (m & 0x1F) { case M_FIQ: return 1; case M_IRQ: return 2; case M_SVC: return 3; case M_ABT: return 4; case M_UND: return 5; default: return 0; } }

static void set_mode(u32 nm) {
    int a = midx(cpsr), b = midx(nm);
    if (a == b) return;
    bank[a].sp = r[13]; bank[a].lr = r[14];
    if (a == 1) { memcpy(bank[1].r8_12, &r[8], 20); memcpy(&r[8], bank[0].r8_12, 20); }
    if (b == 1) { memcpy(bank[0].r8_12, &r[8], 20); memcpy(&r[8], bank[1].r8_12, 20); }
    r[13] = bank[b].sp; r[14] = bank[b].lr;
}
static void set_cpsr(u32 v) { set_mode(v & 0x1F); cpsr = v; }
static u32 get_spsr(void) { int i = midx(cpsr); return i ? spsr[i] : cpsr; }
static int is_priv(void) { return (cpsr & 0x1F) != M_USR; }

static u32 get_user_reg(int i) {
    if (!is_priv()) return r[i];
    if (i == 13) return midx(cpsr) == 0 ? r[13] : bank[0].sp;
    if (i == 14) return midx(cpsr) == 0 ? r[14] : bank[0].lr;
    if (i >= 8 && i <= 12 && midx(cpsr) == 1) return bank[0].r8_12[i - 8];
    return r[i];
}
static void set_user_reg(int i, u32 v) {
    if (!is_priv()) { r[i] = v; return; }
    if (i == 13) { if (midx(cpsr) == 0) r[13] = v; else bank[0].sp = v; return; }
    if (i == 14) { if (midx(cpsr) == 0) r[14] = v; else bank[0].lr = v; return; }
    if (i >= 8 && i <= 12 && midx(cpsr) == 1) { bank[0].r8_12[i - 8] = v; return; }
    r[i] = v;
}

static FILE *disk;
static u32 irq_lines(void);
static u32 dev_read(u32 pa, int *bad);
static void dev_write(u32 pa, u32 v, int size);

static u32 phys_read(u32 pa, int size, int *bad) {
    *bad = 0;
    if (pa < RAM_SIZE) {
        if (size == 4) return *(u32 *)(ram + pa);
        if (size == 2) return *(u16 *)(ram + pa);
        return ram[pa];
    }
    u32 v = dev_read(pa & ~3u, bad);
    if (size == 1) v = (v >> ((pa & 3) * 8)) & 0xFF;
    else if (size == 2) v = (v >> ((pa & 2) * 8)) & 0xFFFF;
    return v;
}
static int phys_write(u32 pa, u32 v, int size) {
    if (pa < RAM_SIZE) {
        if (size == 4) *(u32 *)(ram + pa) = v; else if (size == 2) *(u16 *)(ram + pa) = (u16)v; else ram[pa] = (u8)v;
        return 0;
    }
    dev_write(pa, v, size);
    return 0;
}

static struct { u32 tag, pa, ap; } tlb[256];
static int tlb_ok[256];
static void tlb_flush(void) { memset(tlb_ok, 0, sizeof tlb_ok); }

static u32 xlate(u32 va, int wr, int user, u32 *pa) {
    if (!(sctlr & 1)) { *pa = va; return 0; }
    u32 idx = (va >> 12) & 255, ap, dom, code_base;
    if (tlb_ok[idx] && tlb[idx].tag == va >> 12) {
        *pa = tlb[idx].pa | (va & 0xFFF); ap = tlb[idx].ap;
        goto perm;
    }
    {
        u32 l1a = (ttbr & ~0x3FFFu) | ((va >> 20) << 2);
        if (l1a >= RAM_SIZE) return 0x5;
        u32 d = *(u32 *)(ram + l1a), t = d & 3;
        dom = (d >> 5) & 15;
        u32 pg;
        if (t == 2) { pg = (d & 0xFFF00000u) | (va & 0xFF000); ap = (d >> 10) & 3; code_base = 0; }
        else if (t == 1) {
            u32 l2a = (d & ~0x3FFu) | (((va >> 12) & 0xFF) << 2);
            if (l2a >= RAM_SIZE) return 0x7;
            u32 e = *(u32 *)(ram + l2a);
            if ((e & 3) != 2) return (dom << 4) | 0x7;
            pg = e & ~0xFFFu; ap = (e >> (4 + 2 * ((va >> 10) & 3))) & 3; code_base = 2;
        } else return 0x5;
        u32 da = (dacr >> (2 * dom)) & 3;
        if (da == 0) return (dom << 4) | (code_base ? 0xB : 0x9);
        if (da == 3) ap = 3;
        tlb_ok[idx] = 1; tlb[idx].tag = va >> 12; tlb[idx].pa = pg; tlb[idx].ap = ap;
        *pa = pg | (va & 0xFFF);
        (void)code_base;
    }
perm:
    if (ap == 0) return (0 << 4) | 0xF;
    if (ap == 1 && user) return 0xF;
    if (ap == 2 && user && wr) return 0xF;
    return 0;
}

static int dfault;
static u32 mem_read(u32 va, int size, int user) {
    u32 pa, f = xlate(va, 0, user, &pa);
    if (f) { fsr = f; far_ = va; dfault = 1; return 0; }
    int bad; u32 v = phys_read(pa, size, &bad);
    if (bad) { fsr = 0x8; far_ = va; dfault = 1; }
    return v;
}
static void mem_write(u32 va, u32 v, int size, int user) {
    u32 pa, f = xlate(va, 1, user, &pa);
    if (f) { fsr = f; far_ = va; dfault = 1; return; }
    phys_write(pa, v, size);
}

static u32 sw_irq_pending(void);
static void take_exception(int vec, u32 mode, u32 lr, int mask_f) {
    u32 old = cpsr;
    set_mode(mode);
    spsr[midx(mode)] = old;
    cpsr = (cpsr & ~0x3Fu) | mode | 0x80 | (mask_f ? 0x40 : 0);
    cpsr = (old & 0xF0000000u) | (cpsr & 0x0FFFFFFFu);
    cpsr = (cpsr & ~0x20u);
    r[14] = lr;
    r[15] = ((sctlr & 0x2000) ? 0xFFFF0000u : 0) + vec;
    halted = 0;
}

static inline int flagN(void) { return cpsr >> 31; }
static inline int flagZ(void) { return (cpsr >> 30) & 1; }
static inline int flagC(void) { return (cpsr >> 29) & 1; }
static inline int flagV(void) { return (cpsr >> 28) & 1; }
static void setNZ(u32 v) { cpsr = (cpsr & 0x3FFFFFFFu) | (v & 0x80000000u) | (v == 0 ? 0x40000000u : 0); }
static void setC(int c) { cpsr = (cpsr & ~0x20000000u) | (c ? 0x20000000u : 0); }
static void setV(int c) { cpsr = (cpsr & ~0x10000000u) | (c ? 0x10000000u : 0); }

static int cond_ok(u32 c) {
    switch (c) {
    case 0: return flagZ(); case 1: return !flagZ(); case 2: return flagC(); case 3: return !flagC();
    case 4: return flagN(); case 5: return !flagN(); case 6: return flagV(); case 7: return !flagV();
    case 8: return flagC() && !flagZ(); case 9: return !flagC() || flagZ();
    case 10: return flagN() == flagV(); case 11: return flagN() != flagV();
    case 12: return !flagZ() && flagN() == flagV(); case 13: return flagZ() || flagN() != flagV();
    default: return 1;
    }
}

static u32 shift_op(u32 v, int type, u32 amt, int imm_form, int *co) {
    *co = flagC();
    if (imm_form) {
        if (type == 0) { if (amt == 0) return v; *co = (v >> (32 - amt)) & 1; return v << amt; }
        if (type == 1) { if (amt == 0) amt = 32; if (amt == 32) { *co = v >> 31; return 0; } *co = (v >> (amt - 1)) & 1; return v >> amt; }
        if (type == 2) { if (amt == 0) amt = 32; if (amt >= 32) { *co = v >> 31; return (s32)v >> 31; } *co = (v >> (amt - 1)) & 1; return (u32)((s32)v >> amt); }
        if (amt == 0) { *co = v & 1; return (v >> 1) | ((u32)flagC() << 31); }
        *co = (v >> (amt - 1)) & 1; return (v >> amt) | (v << (32 - amt));
    }
    amt &= 0xFF;
    if (amt == 0) return v;
    switch (type) {
    case 0: if (amt < 32) { *co = (v >> (32 - amt)) & 1; return v << amt; } *co = amt == 32 ? v & 1 : 0; return 0;
    case 1: if (amt < 32) { *co = (v >> (amt - 1)) & 1; return v >> amt; } *co = amt == 32 ? v >> 31 : 0; return 0;
    case 2: if (amt < 32) { *co = (v >> (amt - 1)) & 1; return (u32)((s32)v >> amt); } *co = v >> 31; return (s32)v >> 31;
    default: amt &= 31; if (amt == 0) { *co = v >> 31; return v; } *co = (v >> (amt - 1)) & 1; return (v >> amt) | (v << (32 - amt));
    }
}

static void write_pc(u32 v) { r[15] = v & ~3u; }

static void cp15_write(u32 crn, u32 crm, u32 op2, u32 v) {
    switch (crn) {
    case 1: sctlr = v; tlb_flush(); break;
    case 2: ttbr = v; tlb_flush(); break;
    case 3: dacr = v; tlb_flush(); break;
    case 5: fsr = v; break;
    case 6: far_ = v; break;
    case 7: if (crm == 0 && op2 == 4) halted = 1; break;
    case 8: tlb_flush(); break;
    default: break;
    }
}
static u32 cp15_read(u32 crn, u32 crm, u32 op2) {
    (void)crm;
    switch (crn) {
    case 0: return op2 == 1 ? 0x1D152152u : 0x41069265u;
    case 1: return sctlr;
    case 2: return ttbr;
    case 3: return dacr;
    case 5: return fsr;
    case 6: return far_;
    default: return 0;
    }
}

static u32 bad_inst_pc;
static int step(void);

#define FETCH_FAULT 1
static int exec_one(void) {
    u32 pc = r[15], ipa;
    u32 f = xlate(pc, 0, !is_priv(), &ipa);
    if (f) {
        fsr = f; far_ = pc;
        if (trace) fprintf(stderr, "[pabt pc=%08x fsr=%x]\n", pc, f);
        take_exception(0x0C, M_ABT, pc + 4, 0);
        return 1;
    }
    if (ipa >= RAM_SIZE) { fprintf(stderr, "emu: fetch outside RAM pc=%08x pa=%08x\n", pc, ipa); exit(3); }
    u32 op = *(u32 *)(ram + ipa);
    r[15] = pc + 4;
    u32 cond = op >> 28;
    #define RD(n) ((n) == 15 ? pc + 8 : r[n])
    if (cond == 0xF) {
        if ((op & 0xFE000000u) == 0xFA000000u) { fprintf(stderr, "emu: BLX imm (Thumb) at %08x is not supported\n", pc); exit(3); }
        if ((op & 0xFD70F000u) == 0xF550F000u) return 0;
        goto undef;
    }
    if (!cond_ok(cond)) return 0;
    u32 t = (op >> 25) & 7;
    dfault = 0;
    switch (t) {
    case 0: case 1: {
        if (t == 0 && (op & 0x90) == 0x90) {
            u32 sh = (op >> 5) & 3;
            if (sh == 0) {
                if ((op & 0x0FC000F0u) == 0x00000090u) {
                    u32 rd = (op >> 16) & 15, rn = (op >> 12) & 15, rs = (op >> 8) & 15, rm = op & 15;
                    u32 v = r[rm] * r[rs]; if (op & (1 << 21)) v += r[rn];
                    r[rd] = v; if (op & (1 << 20)) setNZ(v);
                    return 0;
                }
                if ((op & 0x0F8000F0u) == 0x00800090u) {
                    u32 hi = (op >> 16) & 15, lo = (op >> 12) & 15, rs = (op >> 8) & 15, rm = op & 15;
                    u64 v;
                    if (op & (1 << 22)) v = (u64)((s64)(s32)r[rm] * (s64)(s32)r[rs]); else v = (u64)r[rm] * (u64)r[rs];
                    if (op & (1 << 21)) v += ((u64)r[hi] << 32) | r[lo];
                    r[lo] = (u32)v; r[hi] = (u32)(v >> 32);
                    if (op & (1 << 20)) { cpsr = (cpsr & 0x3FFFFFFFu) | ((u32)(v >> 32) & 0x80000000u) | (v == 0 ? 0x40000000u : 0); }
                    return 0;
                }
                if ((op & 0x0FB00FF0u) == 0x01000090u) {
                    u32 rn = (op >> 16) & 15, rd = (op >> 12) & 15, rm = op & 15, a = r[rn], b = (op >> 22) & 1;
                    u32 old = mem_read(a, b ? 1 : 4, !is_priv()); if (dfault) goto dabt;
                    mem_write(a, r[rm], b ? 1 : 4, !is_priv()); if (dfault) goto dabt;
                    r[rd] = old; return 0;
                }
                goto undef;
            }
            u32 P = (op >> 24) & 1, U = (op >> 23) & 1, I = (op >> 22) & 1, W = (op >> 21) & 1, L = (op >> 20) & 1;
            u32 rn = (op >> 16) & 15, rd = (op >> 12) & 15;
            u32 off = I ? (((op >> 4) & 0xF0) | (op & 15)) : r[op & 15];
            u32 base = RD(rn), addr = P ? (U ? base + off : base - off) : base;
            u32 wb = U ? base + off : base - off;
            if (!L && sh >= 2) {
                if (sh == 2) { u32 a = mem_read(addr, 4, 0); if (dfault) goto dabt; u32 b = mem_read(addr + 4, 4, 0); if (dfault) goto dabt; r[rd] = a; r[rd + 1] = b; }
                else { mem_write(addr, r[rd], 4, 0); if (dfault) goto dabt; mem_write(addr + 4, r[rd + 1], 4, 0); if (dfault) goto dabt; }
            } else if (L) {
                u32 v;
                if (sh == 1) { v = mem_read(addr, 2, !is_priv() && 0); if (dfault) goto dabt; }
                else if (sh == 2) { v = mem_read(addr, 1, 0); if (dfault) goto dabt; v = (u32)(s32)(int8_t)v; }
                else { v = mem_read(addr, 2, 0); if (dfault) goto dabt; v = (u32)(s32)(int16_t)v; }
                r[rd] = v;
            } else {
                mem_write(addr, r[rd], 2, 0); if (dfault) goto dabt;
            }
            if (!P || W) r[rn] = wb;
            if (L && rd == 15) write_pc(r[15]);
            return 0;
        }
        if (t == 0 && (op & 0x0FFFFFF0u) == 0x012FFF10u) { u32 v = r[op & 15]; if (v & 1) { fprintf(stderr, "emu: BX to Thumb pc=%08x\n", pc); exit(3); } write_pc(v); return 0; }
        if (t == 0 && (op & 0x0FFFFFF0u) == 0x012FFF30u) { u32 v = r[op & 15]; r[14] = pc + 4; if (v & 1) { fprintf(stderr, "emu: BLX Thumb pc=%08x\n", pc); exit(3); } write_pc(v); return 0; }
        if (t == 0 && (op & 0x0FFF0FF0u) == 0x016F0F10u) { u32 v = r[op & 15], n = 0; if (!v) n = 32; else while (!(v & 0x80000000u)) { v <<= 1; n++; } r[(op >> 12) & 15] = n; return 0; }
        if (t == 0 && (op & 0x0FBF0FFFu) == 0x010F0000u) { r[(op >> 12) & 15] = (op & (1 << 22)) ? get_spsr() : cpsr; return 0; }
        if ((t == 0 && (op & 0x0FB0FFF0u) == 0x0120F000u) || (t == 1 && (op & 0x0FB0F000u) == 0x0320F000u)) {
            u32 v;
            if (t == 1) { u32 imm = op & 0xFF, rot = ((op >> 8) & 15) * 2; v = rot ? (imm >> rot) | (imm << (32 - rot)) : imm; } else v = r[op & 15];
            u32 mask = 0;
            if (op & (1 << 16)) mask |= 0x000000FF; if (op & (1 << 17)) mask |= 0x0000FF00;
            if (op & (1 << 18)) mask |= 0x00FF0000; if (op & (1 << 19)) mask |= 0xFF000000;
            if (op & (1 << 22)) { int i = midx(cpsr); if (i) spsr[i] = (spsr[i] & ~mask) | (v & mask); }
            else {
                if (!is_priv()) mask &= 0xFF000000u;
                set_cpsr((cpsr & ~mask) | (v & mask));
            }
            return 0;
        }
        u32 opc = (op >> 21) & 15, S = (op >> 20) & 1, rn = (op >> 16) & 15, rd = (op >> 12) & 15, a, b; int co;
        if (t == 1) { u32 imm = op & 0xFF, rot = ((op >> 8) & 15) * 2; b = rot ? (imm >> rot) | (imm << (32 - rot)) : imm; co = rot ? (b >> 31) : flagC(); a = RD(rn); }
        else {
            u32 rm = op & 15, type = (op >> 5) & 3;
            if (op & 0x10) { u32 rs = (op >> 8) & 15; a = rn == 15 ? pc + 12 : r[rn]; u32 mv = rm == 15 ? pc + 12 : r[rm]; b = shift_op(mv, type, r[rs], 0, &co); }
            else { a = RD(rn); b = shift_op(RD(rm), type, (op >> 7) & 31, 1, &co); }
        }
        u32 res = 0; int logical = 0, wr_res = 1; u64 w;
        int cin = flagC();
        switch (opc) {
        case 0: res = a & b; logical = 1; break;
        case 1: res = a ^ b; logical = 1; break;
        case 2: res = a - b; if (S) { setC(a >= b); setV(((a ^ b) & (a ^ res)) >> 31); } break;
        case 3: res = b - a; if (S) { setC(b >= a); setV(((b ^ a) & (b ^ res)) >> 31); } break;
        case 4: w = (u64)a + b; res = (u32)w; if (S) { setC(w >> 32); setV((~(a ^ b) & (a ^ res)) >> 31); } break;
        case 5: w = (u64)a + b + cin; res = (u32)w; if (S) { setC(w >> 32); setV((~(a ^ b) & (a ^ res)) >> 31); } break;
        case 6: w = (u64)a + (u32)~b + cin; res = (u32)w; if (S) { setC(w >> 32); setV(((a ^ b) & (a ^ res)) >> 31); } break;
        case 7: w = (u64)b + (u32)~a + cin; res = (u32)w; if (S) { setC(w >> 32); setV(((b ^ a) & (b ^ res)) >> 31); } break;
        case 8: res = a & b; logical = 1; wr_res = 0; break;
        case 9: res = a ^ b; logical = 1; wr_res = 0; break;
        case 10: res = a - b; wr_res = 0; setC(a >= b); setV(((a ^ b) & (a ^ res)) >> 31); S = 1; break;
        case 11: w = (u64)a + b; res = (u32)w; wr_res = 0; setC(w >> 32); setV((~(a ^ b) & (a ^ res)) >> 31); S = 1; break;
        case 12: res = a | b; logical = 1; break;
        case 13: res = b; logical = 1; break;
        case 14: res = a & ~b; logical = 1; break;
        default: res = ~b; logical = 1; break;
        }
        if (wr_res && rd == 15 && (op & (1 << 20)) && opc < 8 + 0 ) { set_cpsr(get_spsr()); write_pc(res); return 0; }
        if (wr_res && rd == 15 && (op & (1 << 20))) { set_cpsr(get_spsr()); write_pc(res); return 0; }
        if (S) { setNZ(res); if (logical) setC(co); }
        if (wr_res) { if (rd == 15) write_pc(res); else r[rd] = res; }
        return 0;
    }
    case 2: case 3: {
        if (t == 3 && (op & 0x10)) goto undef;
        u32 P = (op >> 24) & 1, U = (op >> 23) & 1, B = (op >> 22) & 1, W = (op >> 21) & 1, L = (op >> 20) & 1;
        u32 rn = (op >> 16) & 15, rd = (op >> 12) & 15, off;
        if (t == 2) off = op & 0xFFF;
        else { int co; off = shift_op(RD(op & 15), (op >> 5) & 3, (op >> 7) & 31, 1, &co); }
        u32 base = RD(rn), wb = U ? base + off : base - off, addr = P ? wb : base;
        int user = (!P && W);
        if (L) {
            u32 v = mem_read(addr & (B ? ~0u : ~3u), B ? 1 : 4, user);
            if (dfault) goto dabt;
            if (!B && (addr & 3)) { u32 rot = (addr & 3) * 8; v = (v >> rot) | (v << (32 - rot)); }
            if (!P || W) r[rn] = wb;
            if (rd == 15) write_pc(v); else r[rd] = v;
        } else {
            u32 v = rd == 15 ? pc + 12 : r[rd];
            if (B) mem_write(addr, v & 0xFF, 1, user); else mem_write(addr & ~3u, v, 4, user);
            if (dfault) goto dabt;
            if (!P || W) r[rn] = wb;
        }
        return 0;
    }
    case 4: {
        u32 P = (op >> 24) & 1, U = (op >> 23) & 1, S = (op >> 22) & 1, W = (op >> 21) & 1, L = (op >> 20) & 1;
        u32 rn = (op >> 16) & 15, list = op & 0xFFFF, n = __builtin_popcount(list), base = r[rn], start, wb;
        if (U) { start = P ? base + 4 : base; wb = base + 4 * n; } else { start = P ? base - 4 * n : base - 4 * n + 4; wb = base - 4 * n; }
        int user_regs = S && !(L && (list & 0x8000));
        u32 a = start;
        for (int i = 0; i < 16; i++) {
            if (!(list & (1u << i))) continue;
            if (L) {
                u32 v = mem_read(a, 4, 0); if (dfault) goto dabt;
                if (i == 15) { if (S) set_cpsr(get_spsr()); write_pc(v); }
                else if (user_regs) set_user_reg(i, v); else r[i] = v;
            } else {
                u32 v = user_regs ? get_user_reg(i) : (i == 15 ? pc + 12 : r[i]);
                mem_write(a, v, 4, 0); if (dfault) goto dabt;
            }
            a += 4;
        }
        if (W && !(L && (list & (1u << rn)))) r[rn] = wb;
        return 0;
    }
    case 5: {
        s32 off = ((s32)(op << 8)) >> 6;
        if (op & (1 << 24)) r[14] = pc + 4;
        r[15] = pc + 8 + off;
        return 0;
    }
    case 7:
        if ((op & 0x0F000000u) == 0x0F000000u) { if (trace) fprintf(stderr, "[swi pc=%08x r7=%u]\n", pc, r[7]); take_exception(0x08, M_SVC, pc + 4, 0); return 1; }
        if ((op & 0x0F000010u) == 0x0E000010u && ((op >> 8) & 15) == 15) {
            u32 rd = (op >> 12) & 15, crn = (op >> 16) & 15, crm = op & 15, op2 = (op >> 5) & 7;
            if (!is_priv()) goto undef;
            if (op & (1 << 20)) { u32 v = cp15_read(crn, crm, op2); if (rd == 15) { cpsr = (cpsr & 0x0FFFFFFFu) | (v & 0xF0000000u); } else r[rd] = v; }
            else cp15_write(crn, crm, op2, rd == 15 ? pc + 8 : r[rd]);
            return 0;
        }
        goto undef;
    default: goto undef;
    }
undef:
    if (trace) fprintf(stderr, "[undef pc=%08x op=%08x]\n", pc, op);
    take_exception(0x04, M_UND, pc + 4, 0);
    return 1;
dabt:
    if (trace) fprintf(stderr, "[dabt pc=%08x far=%08x fsr=%x]\n", pc, far_, fsr);
    r[15] = pc;
    take_exception(0x10, M_ABT, pc + 8, 0);
    return 1;
}

#define UART0 0x101F1000u
#define TIMER 0x101E2000u
static u32 sys_lock;
static u32 vic_enable, vic_soft, vic_select;
static u32 sic_enable;
static struct { u32 load, value, ctrl, ris; } tm[2];
static u64 timer_rem;
static u32 rtc_off;
typedef struct { u32 cr; u8 q[64]; int qh, qt; int state; int sr_seq; int wheel; int enabled; } Kmi;
static Kmi kmi[2];
static int kq_count(Kmi *k) { return (k->qt - k->qh + 64) & 63; }
static void kq_push(Kmi *k, u8 b) { if (kq_count(k) < 63) { k->q[k->qt] = b; k->qt = (k->qt + 1) & 63; } }
static u8 kq_pop(Kmi *k) { u8 b = k->q[k->qh]; if (k->qh != k->qt) k->qh = (k->qh + 1) & 63; return b; }
static int kmi_irq(Kmi *k) { return (k->cr & 0x10) && kq_count(k) > 0; }

static void kmi_cmd(int which, u8 b) {
    Kmi *k = &kmi[which];
    if (k->state) {
        if (which == 1 && k->state == 0xF3) { k->sr_seq = (k->sr_seq == 0 && b == 200) ? 1 : (k->sr_seq == 1 && b == 100) ? 2 : (k->sr_seq == 2 && b == 80) ? 3 : 0; if (k->sr_seq == 3) k->wheel = 1; }
        k->state = 0; kq_push(k, 0xFA); return;
    }
    switch (b) {
    case 0xFF: kq_push(k, 0xFA); kq_push(k, 0xAA); if (which) kq_push(k, 0x00); k->wheel = 0; k->sr_seq = 0; break;
    case 0xF2: kq_push(k, 0xFA); kq_push(k, which ? (k->wheel ? 3 : 0) : 0xAB); if (!which) kq_push(k, 0x83); break;
    case 0xF3: case 0xED: case 0xF0: kq_push(k, 0xFA); k->state = b; break;
    case 0xF4: k->enabled = 1; kq_push(k, 0xFA); break;
    default: kq_push(k, 0xFA); break;
    }
}
static u32 sic_raw(void) { return (kmi_irq(&kmi[0]) ? 1u << 3 : 0) | (kmi_irq(&kmi[1]) ? 1u << 4 : 0); }
static u32 vic_raw(void) {
    u32 v = vic_soft;
    if ((tm[0].ctrl & 0x20) && tm[0].ris) v |= 1u << 4;
    if (sic_raw() & sic_enable) v |= 1u << 31;
    return v;
}
static u32 vic_status(void) { return vic_raw() & vic_enable & ~vic_select; }
static u32 irq_lines(void) { return vic_status(); }

static u32 sd_reg[0x40], sd_status, sd_fifo[128]; static int sd_fn, sd_fh, sd_wr, sd_app; static u32 sd_wlba;
static u32 clcd_reg[8]; static u32 clcd_pal[128];
static u32 sd_resp[4];

static void sd_command(u32 cmd) {
    u32 idx = cmd & 0x3F, arg = sd_reg[2], lr = (cmd >> 7) & 1, resp = (cmd >> 6) & 1;
    memset(sd_resp, 0, sizeof sd_resp);
    int app = sd_app; sd_app = 0;
    switch (idx) {
    case 0: break;
    case 8: sd_resp[0] = arg; break;
    case 55: sd_resp[0] = 0x120; sd_app = 1; break;
    case 41: if (app) sd_resp[0] = 0x80FF8000u; break;
    case 2: sd_resp[0] = 0x1D534D49; sd_resp[1] = 0x4D414745; sd_resp[2] = 0x30010203; sd_resp[3] = 0x04010001; break;
    case 3: sd_resp[0] = 0x45670000u | 0x500; break;
    case 9: {
        u32 csd[4] = {0, 0, 0, 0};
        #define SETB(lo, len, val) do { for (int _i = 0; _i < (len); _i++) if (((val) >> _i) & 1) csd[3 - ((lo) + _i) / 32] |= 1u << (((lo) + _i) % 32); } while (0)
        SETB(80, 4, 9u); SETB(62, 12, 31u); SETB(47, 3, 7u);
        memcpy(sd_resp, csd, 16); break; }
    case 7: case 16: sd_resp[0] = 0x900; break;
    case 17: {
        u8 buf[512]; fseek(disk, (long)arg, SEEK_SET); if (fread(buf, 1, 512, disk) != 512) memset(buf, 0, 512);
        memcpy(sd_fifo, buf, 512); sd_fn = 128; sd_fh = 0; sd_wr = 0; sd_resp[0] = 0x900; break; }
    case 24: sd_wlba = arg; sd_fn = 0; sd_fh = 0; sd_wr = 1; sd_resp[0] = 0x900; break;
    default: sd_resp[0] = 0x900; break;
    }
    (void)lr;
    sd_status |= resp ? (1u << 6) : (1u << 7);
}
static u32 sd_fifo_status(void) {
    u32 s = sd_status;
    if (!sd_wr && sd_fh < sd_fn) s |= 1u << 21;
    if (sd_wr) s |= 1u << 18;
    return s;
}

static u32 dev_read(u32 pa, int *bad) {
    u32 o;
    *bad = 0;
    if (pa >= 0x10000000u && pa < 0x10001000u) { o = pa & 0xFFF; return o == 0 ? 0x41007004u : o == 0x5C ? (u32)(icount / 2) : 0; }
    if (pa >= 0x10003000u && pa < 0x10004000u) { o = pa & 0xFFF; if (o == 0) return sic_raw() & sic_enable; if (o == 4) return sic_raw(); if (o == 8) return sic_enable; return 0; }
    if (pa >= 0x10005000u && pa < 0x10006000u) {
        o = pa & 0xFFF;
        if (o == 0x34) return sd_fifo_status();
        if (o >= 0x14 && o <= 0x20) return sd_resp[(o - 0x14) / 4];
        if (o == 0x80) { u32 v = sd_fh < 128 ? sd_fifo[sd_fh++] : 0; if (sd_fh >= 128 && !sd_wr) sd_status |= (1u << 8) | (1u << 10); return v; }
        if (o == 0x3C) return sd_reg[0x3C / 4];
        return sd_reg[o / 4 & 0x3F];
    }
    if (pa >= 0x10006000u && pa < 0x10008000u) {
        Kmi *k = &kmi[(pa >> 12) & 1]; o = pa & 0xFFF;
        if (o == 0) return k->cr;
        if (o == 4) return 0x40 | (kq_count(k) ? 0x10 : 0);
        if (o == 8) return kq_count(k) ? kq_pop(k) : 0;
        if (o == 0x10) return kmi_irq(k) ? 2 : 0;
        return 0;
    }
    if (pa >= 0x10120000u && pa < 0x10121000u) { o = pa & 0xFFF; if (o < 0x20) return clcd_reg[o / 4]; if (o >= 0x200 && o < 0x400) return clcd_pal[(o - 0x200) / 4]; return 0; }
    if (pa >= 0x10140000u && pa < 0x10141000u) {
        o = pa & 0xFFF;
        switch (o) { case 0: return vic_status(); case 4: return vic_raw() & vic_enable & vic_select; case 8: return vic_raw(); case 0xC: return vic_select; case 0x10: return vic_enable; case 0x18: return vic_soft; default: return 0; }
    }
    if (pa >= TIMER && pa < TIMER + 0x1000) {
        int n = (pa & 0x20) ? 1 : 0; o = pa & 0x1F;
        switch (o) { case 0: return tm[n].load; case 4: return tm[n].value; case 8: return tm[n].ctrl; case 0x10: return tm[n].ris; case 0x14: return (tm[n].ctrl & 0x20) ? tm[n].ris : 0; default: return 0; }
    }
    if (pa >= 0x101E8000u && pa < 0x101E9000u) { o = pa & 0xFFF; if (o == 0) return (u32)time(NULL) + rtc_off; return 0; }
    if (pa >= UART0 && pa < UART0 + 0x1000) { o = pa & 0xFFF; if (o == 0x18) return 0x90; if (o == 0) return 0; return 0; }
    static int warned[256]; int pg = (pa >> 12) & 255;
    if ((pa >> 20) == 0x101 || (pa >> 20) == 0x100) return 0;
    if (!warned[pg]) { warned[pg] = 1; fprintf(stderr, "emu: read from unmapped address %08x\n", pa); }
    *bad = 1; return 0;
}

static FILE *outf;
static void dev_write(u32 pa, u32 v, int size) {
    u32 o;
    if (size == 2 && (pa >> 20) == 0x101 && (pa & 0xFFFFF000u) == 0x10120000u) {
        o = pa & 0xFFF;
        if (o >= 0x200 && o < 0x400) { u32 w = (o - 0x200) / 4; int sh = (o & 2) * 8; (void)sh; clcd_pal[w] = v & 0xFFFF; return; }
    }
    if (pa >= 0x10000000u && pa < 0x10001000u) {
        o = pa & 0xFFF;
        if (o == 0x20) { sys_lock = v == 0xA05F ? v : (v & 0x7FFF); return; }
        if (o == 0x40) {
            if (sys_lock == 0xA05F && (v & 0x100)) { fprintf(stderr, "emu: SYS_RESETCTL=%08x -> board reset (QEMU -no-reboot: exit)\n", v); fflush(stdout); exit(0); }
            fprintf(stderr, "emu: SYS_RESETCTL=%08x IGNORED (lock=%04x, bit 8=%d): QEMU would not reset\n", v, sys_lock, (v >> 8) & 1); return;
        }
        return;
    }
    if (pa >= 0x10003000u && pa < 0x10004000u) { o = pa & 0xFFF; if (o == 8) sic_enable |= v; else if (o == 0xC) sic_enable &= ~v; return; }
    if (pa >= 0x10005000u && pa < 0x10006000u) {
        o = pa & 0xFFF;
        if (o == 0x0C) { if (v & (1u << 10)) sd_command(v); sd_reg[3] = v; return; }
        if (o == 0x38) { sd_status &= ~(v & 0x7FF); return; }
        if (o == 0x2C) { sd_reg[0x2C / 4] = v; return; }
        if (o == 0x80) {
            if (sd_wr && sd_fn < 128) {
                sd_fifo[sd_fn++] = v;
                if (sd_fn == 128) { fseek(disk, (long)sd_wlba, SEEK_SET); fwrite(sd_fifo, 1, 512, disk); fflush(disk); sd_status |= (1u << 8) | (1u << 10); sd_wr = 0; sd_fn = 0; }
            }
            return;
        }
        sd_reg[(o / 4) & 0x3F] = v; return;
    }
    if (pa >= 0x10006000u && pa < 0x10008000u) {
        int w = (pa >> 12) & 1; Kmi *k = &kmi[w]; o = pa & 0xFFF;
        if (o == 0) k->cr = v; else if (o == 8) kmi_cmd(w, (u8)v);
        return;
    }
    if (pa >= 0x10120000u && pa < 0x10121000u) { o = pa & 0xFFF; if (o < 0x20) clcd_reg[o / 4] = v; else if (o >= 0x200 && o < 0x400) clcd_pal[(o - 0x200) / 4] = v; return; }
    if (pa >= 0x10140000u && pa < 0x10141000u) {
        o = pa & 0xFFF;
        if (o == 0xC) vic_select = v; else if (o == 0x10) vic_enable |= v; else if (o == 0x14) vic_enable &= ~v; else if (o == 0x18) vic_soft |= v; else if (o == 0x1C) vic_soft &= ~v;
        return;
    }
    if (pa >= TIMER && pa < TIMER + 0x1000) {
        int n = (pa & 0x20) ? 1 : 0; o = pa & 0x1F;
        switch (o) {
        case 0: tm[n].load = v; tm[n].value = v; break;
        case 8: tm[n].ctrl = v; break;
        case 0xC: tm[n].ris = 0; break;
        case 0x18: tm[n].load = v; break;
        }
        return;
    }
    if (pa >= 0x101E8000u && pa < 0x101E9000u) { o = pa & 0xFFF; if (o == 8) rtc_off = v - (u32)time(NULL); return; }
    if (pa >= UART0 && pa < UART0 + 0x1000) { if ((pa & 0xFFF) == 0) { fputc((int)(v & 0xFF), stdout); fflush(stdout); } return; }
    if ((pa >> 20) == 0x101 || (pa >> 20) == 0x100) return;
    static int warned2; if (!warned2++) fprintf(stderr, "emu: write to unmapped address %08x\n", pa);
}

static void timers_advance(u64 instrs) {
    timer_rem += instrs;
    u64 ticks = timer_rem / 40; timer_rem %= 40;
    for (int n = 0; n < 2; n++) {
        if (!(tm[n].ctrl & 0x80) || !ticks) continue;
        u64 val = tm[n].value;
        if (ticks >= val) {
            tm[n].ris = 1;
            if (tm[n].ctrl & 0x40) { u32 l = tm[n].load ? tm[n].load : 1; u64 rest = ticks - val; tm[n].value = (u32)(l - (rest % l)); }
            else tm[n].value = 0xFFFFFFFFu - (u32)(ticks - val);
        } else tm[n].value = (u32)(val - ticks);
    }
}

static void dump_screen(const char *file) {
    u32 base = clcd_reg[4], cr = clcd_reg[6];
    FILE *f = fopen(file, "wb"); if (!f) return;
    fprintf(f, "P6\n640 480\n255\n");
    for (int i = 0; i < 640 * 480; i++) {
        u8 p = (cr & 1) && base < RAM_SIZE ? ram[base + i] : 0;
        u32 e = (clcd_pal[p / 2] >> ((p & 1) * 16)) & 0xFFFF;
        u8 c[3] = { (u8)((e & 31) << 3), (u8)(((e >> 5) & 31) << 3), (u8)(((e >> 10) & 31) << 3) };
        if (cr & 0x100) { u8 t = c[0]; c[0] = c[2]; c[2] = t; }
        fwrite(c, 1, 3, f);
    }
    fclose(f);
}

typedef struct { u64 at; int kind; u8 d[8]; int n; } Inj;
static Inj inj[256]; static int ninj;
typedef struct { u64 at; char file[128]; } Shot;
static Shot shots[32]; static int nshots;

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: armemu kernel.bin disk.img [-n Minsn] [-shot M file] [-key M hex,hex] [-mouse M dx dy btn] [-trace]\n"); return 2; }
    u64 maxi = 400ull * 1000000;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-n")) maxi = (u64)(atof(argv[++i]) * 1e6);
        else if (!strcmp(argv[i], "-trace")) trace = 1;
        else if (!strcmp(argv[i], "-shot")) { shots[nshots].at = (u64)(atof(argv[i + 1]) * 1e6); snprintf(shots[nshots].file, 128, "%s", argv[i + 2]); nshots++; i += 2; }
        else if (!strcmp(argv[i], "-key")) {
            Inj *j = &inj[ninj++]; j->at = (u64)(atof(argv[i + 1]) * 1e6); j->kind = 0; j->n = 0;
            char *s = strdup(argv[i + 2]); for (char *t = strtok(s, ","); t && j->n < 8; t = strtok(NULL, ",")) j->d[j->n++] = (u8)strtol(t, 0, 16);
            i += 2;
        } else if (!strcmp(argv[i], "-mouse")) {
            Inj *j = &inj[ninj++]; j->at = (u64)(atof(argv[i + 1]) * 1e6); j->kind = 1; j->n = 3;
            int dx = atoi(argv[i + 2]), dy = atoi(argv[i + 3]), b = atoi(argv[i + 4]);
            j->d[0] = (u8)(8 | (b & 7) | (dx < 0 ? 0x10 : 0) | (dy < 0 ? 0x20 : 0)); j->d[1] = (u8)dx; j->d[2] = (u8)dy; i += 4;
        }
    }
    ram = calloc(1, RAM_SIZE);
    FILE *k = fopen(argv[1], "rb"); if (!k) { perror("kernel"); return 2; }
    size_t n = fread(ram + 0x10000, 1, RAM_SIZE - 0x10000, k); fclose(k);
    disk = fopen(argv[2], "r+b"); if (!disk) { perror("disk"); return 2; }
    u32 *atag = (u32 *)(ram + 0x100);
    atag[0] = 2; atag[1] = 0x54410001; atag[2] = 4; atag[3] = 0x54410002; atag[4] = RAM_SIZE; atag[5] = 0; atag[6] = 0; atag[7] = 0;
    cpsr = M_SVC | 0xC0; r[0] = 0; r[1] = 0x183; r[2] = 0x100; r[15] = 0x10000;
    fprintf(stderr, "emu: kernel %zu bytes at 0x10000, starting\n", n);
    u64 next_dev = 0; int si = 0;
    while (icount < maxi) {
        if (halted) {
            timers_advance(200); icount += 200;
            if (irq_lines()) halted = 0;
        } else {
            exec_one(); icount++;
        }
        if ((icount & 63) == 0 || halted) {
            if (!halted) timers_advance(64);
            if (!(cpsr & 0x80) && irq_lines()) { halted = 0; take_exception(0x18, M_IRQ, r[15] + 4, 0); }
        }
        if (icount >= next_dev) {
            next_dev = icount + 100000;
            for (int i = 0; i < ninj; i++) if (inj[i].at && icount >= inj[i].at) {
                Kmi *kk = &kmi[inj[i].kind]; for (int j = 0; j < inj[i].n; j++) kq_push(kk, inj[i].d[j]);
                if (inj[i].kind == 1 && kk->wheel) kq_push(kk, 0);
                inj[i].at = 0;
            }
            for (; si < nshots; si++) { if (icount < shots[si].at) break; dump_screen(shots[si].file); fprintf(stderr, "emu: screenshot %s @%lluM\n", shots[si].file, (unsigned long long)(icount / 1000000)); }
        }
    }
    dump_screen("armemu.ppm");
    fprintf(stderr, "emu: done after %llu instructions, pc=%08x cpsr=%08x\n", (unsigned long long)icount, r[15], cpsr);
    return 0;
}
