#ifndef GOS_ABI_H
#define GOS_ABI_H
#include "lib/types.h"

enum { VN_FILE = 0, VN_DIR = 1, VN_DEV = 2, VN_PIPE = 3 };
enum { O_RDONLY = 0, O_WRONLY = 1, O_RDWR = 2, O_ACCMODE = 3, O_CREAT = 0x40, O_TRUNC = 0x200, O_APPEND = 0x400, O_NONBLOCK = 0x800 };
enum { SEEK_SET = 0, SEEK_CUR = 1, SEEK_END = 2 };

#define GOS_VERSION "0.6"
#define GOS_UID_ROOT 0
#define GOS_UID_USER 1000

#define GOS_NAME_MAX 28
#define GOS_KLOG_MAX 16384
#define GOS_CLIP_MAX (64 * 1024)
typedef struct { u8 type, virt; u16 mode; u32 size, mtime, ino; u16 uid, gid; } GStat;
typedef struct { char name[GOS_NAME_MAX]; u8 type; u8 pad[3]; u32 size; } GDirEnt;

enum {
    KEY_UP = 0x100, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN,
    KEY_DEL, KEY_INS, KEY_F1 = 0x110, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10
};
enum { MOD_SHIFT = 1, MOD_CTRL = 2, MOD_ALT = 4 };

#define GOS_MAXTASK 32
enum { WNOHANG = 1 };
enum { PIPE_RD_NONBLOCK = 1, PIPE_WR_NONBLOCK = 2 };
enum { SPAWN_DETACH = 1 };
#define GOS_FD_INHERIT (-1)
typedef struct { int pid, state, user; u32 cpu, mem_kb; char name[16]; int ppid, uid, exit_code; } GProcInfo;
typedef struct {
    u32 ram_kb, heap_total, heap_used;
    u32 pmm_total, pmm_free;
    u32 ticks, hz, cpu_load;
    u32 ata, fs_dirty, fs_sync_bytes;
    u32 vfs_gen, vfs_files, vfs_dirs, vfs_bytes;
    u32 nwin;
    int fs_sync_pid;
} GSysInfo;

enum { SET_DESK_COLOR = 0, SET_DESK_PATTERN, SET_MOUSE_SPEED, SET_DBLCLICK, SET_KEY_DELAY, SET_KEY_RATE, SET_VOLUME, SET_COUNT };
#define GOS_DESK_COLORS   6
#define GOS_DESK_PATTERNS 8

#define GOS_TRASH_DIR "/home/user/Trash"
enum { TRASH_MOVE = 1, TRASH_PUTAWAY, TRASH_EMPTY, TRASH_INFO };

enum { GE_NONE = 0, GE_KEY, GE_MOUSE_DOWN, GE_MOUSE_UP, GE_MOUSE_MOVE, GE_SCROLL, GE_CMD, GE_CLOSE, GE_RESIZE,
       GE_DROP  };
typedef struct { int type, a, b, c, d; } GEvent;
typedef struct { int id, cw, ch, pitch, x, y, w, h, zoomed; u32 surf; } GWinInfo;
enum { WC_TITLE = 1, WC_INFO, WC_SCROLL, WC_MENU, WC_MINSIZE, WC_EDITMENU };
#define GOS_SURF_W 640
#define GOS_SURF_H 480

enum {
    SYS_EXIT = 1, SYS_READ = 3, SYS_WRITE = 4, SYS_OPEN = 5, SYS_CLOSE = 6, SYS_UNLINK = 10,
    SYS_CHDIR = 12, SYS_TIME = 13, SYS_CHMOD = 15, SYS_LSEEK = 19, SYS_GETPID = 20, SYS_SYNC = 36,
    SYS_KILL = 37,    SYS_RENAME = 38, SYS_MKDIR = 39, SYS_RMDIR = 40, SYS_BRK = 45, SYS_STAT = 106,
    SYS_WAIT = 7,
    SYS_DUP = 41, SYS_PIPE = 42,
    SYS_DUP2 = 63, SYS_GETUID = 24, SYS_GETGID = 47,
    SYS_CHOWN = 182,
    SYS_YIELD = 158, SYS_SLEEPMS = 162, SYS_GETCWD = 183, SYS_FSTAT = 184,
    SYS_READDIR = 300,
    SYS_SPAWN = 301,
    SYS_PROCINFO = 302,
    SYS_SYSINFO = 303,
    SYS_KLOG = 304,
    SYS_REBOOT = 305, SYS_POWEROFF = 306,
    SYS_LAUNCH = 307,
    SYS_GETFONT = 308,
    SYS_TICKS = 309,
    SYS_GETUIFONT = 310,
    SYS_WIN_CREATE = 320,
    SYS_WIN_INFO = 321,
    SYS_WIN_EVENT = 322,
    SYS_WIN_PRESENT = 323,
    SYS_WIN_CTL = 324,
    SYS_WIN_CLOSE = 325,
    SYS_WIN_MSGBOX = 326,
    SYS_SETTING_GET = 340,
    SYS_SETTING_SET = 341,
    SYS_SETTIME = 342,
    SYS_BEEP = 343,
    SYS_TRASH = 344,
    SYS_CONFIRM = 345,
    SYS_DRAG_BEGIN = 346,
    SYS_DRAG_GET = 347,
    SYS_DUPLICATE = 348,
    SYS_CLIP_SET = 330,
    SYS_CLIP_GET = 331
};

#endif
