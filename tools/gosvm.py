#!/usr/bin/env python3
"""gosvm: GOS GUI test harness driven through the QEMU monitor (headless).

- GosVM        : boots the image, sends keys/mouse/wheel, takes screendumps, reads the serial log.
- read_vfs()   : parses the GOSFS002 snapshot from the disk image (ground truth for the VFS contents after 'Save to Disk').
- Img          : ppm -> pixels (no PIL needed for comparisons; PIL only for saving PNG).

Use as a library from tools/gui_test.py. Needs qemu-system-i386 and python3.
"""
import os, shutil, socket, struct, subprocess, tempfile, time

FS_LBA, FS_SECTORS = 2048, 2048
SW, SH = 640, 480

# QEMU key names for ASCII characters
_SHIFTED = {'!': '1', '@': '2', '#': '3', '$': '4', '%': '5', '^': '6', '&': '7', '*': '8', '(': '9', ')': '0',
            '_': 'minus', '+': 'equal', '{': 'bracket_left', '}': 'bracket_right', ':': 'semicolon', '"': 'apostrophe',
            '<': 'comma', '>': 'dot', '?': 'slash', '|': 'backslash', '~': 'grave_accent'}
_PLAIN = {' ': 'spc', '\n': 'ret', '\t': 'tab', '-': 'minus', '=': 'equal', '[': 'bracket_left', ']': 'bracket_right',
          ';': 'semicolon', "'": 'apostrophe', ',': 'comma', '.': 'dot', '/': 'slash', '\\': 'backslash', '`': 'grave_accent'}


def read_vfs(img_path):
    """Return {path: dict(type, mode, uid, gid, size, data)} from the newest valid snapshot, or None."""
    best = None
    with open(img_path, 'rb') as f:
        for s in range(2):
            f.seek((FS_LBA + s * FS_SECTORS) * 512)
            h = f.read(512)
            if h[:8] != b'GOSFS002':
                continue
            ln, _sum, seq, _hs = struct.unpack_from('<IIII', h, 8)
            f.seek((FS_LBA + s * FS_SECTORS + 1) * 512)
            payload = f.read(ln)
            if best is None or seq > best[0]:
                best = (seq, payload)
    if best is None:
        return None
    p, out, i = best[1], {}, 0
    while i + 20 <= len(p):
        plen, typ, _pad, mode, uid, gid, _pad2, mtime, size = struct.unpack_from('<HBBHHHHII', p, i)
        i += 20
        path = p[i:i + plen].decode('latin1'); i += plen
        data = b''
        if typ == 0:                              # VN_FILE (abi.h: VN_FILE=0, VN_DIR=1); directories carry no data
            data = p[i:i + size]; i += size
        out[path] = dict(type=typ, mode=mode, uid=uid, gid=gid, size=size, data=data)
    return out


class GosVM:
    def __init__(self, img, mem=16, tag='vm', keep_img=False, qemu='qemu-system-i386', extra=()):
        self.dir = tempfile.mkdtemp(prefix='gosvm_' + tag + '_')
        self.img = os.path.join(self.dir, 'disk.img')
        shutil.copyfile(img, self.img)
        self.sock_path = os.path.join(self.dir, 'mon.sock')
        self.serial = os.path.join(self.dir, 'serial.log')
        self.mem = mem
        self.proc = subprocess.Popen(
            [qemu, '-m', str(mem), '-drive', 'format=raw,file=' + self.img, '-vga', 'std', '-display', 'none',
             '-serial', 'file:' + self.serial, '-monitor', 'unix:%s,server,nowait' % self.sock_path, '-no-reboot', *extra],
            stdout=subprocess.DEVNULL, stderr=open(os.path.join(self.dir, 'qemu.err'), 'wb'))
        self.s = socket.socket(socket.AF_UNIX)
        for _ in range(100):
            try:
                self.s.connect(self.sock_path); break
            except OSError:
                time.sleep(0.1)
        else:
            raise RuntimeError('QEMU monitor did not come up')
        self.s.settimeout(5)
        self._drain()
        self.mx, self.my = SW // 2, SH // 2           # GOS cursor position (assumed centered at boot)
        self.nshot = 0

    # ---- monitor ----
    def _drain(self, wait=0.05):
        time.sleep(wait)
        self.s.setblocking(False)
        try:
            while self.s.recv(65536):
                pass
        except (BlockingIOError, OSError):
            pass
        self.s.settimeout(5)

    def cmd(self, c, wait=0.03):
        self.s.sendall((c + '\n').encode())
        self._drain(wait)

    def log(self):
        try:
            return open(self.serial, 'rb').read().decode('latin1').replace('\r', '')
        except OSError:
            return ''

    def wait_log(self, needle, timeout=40):
        t = time.time()
        while time.time() - t < timeout:
            if needle in self.log():
                return True
            time.sleep(0.25)
        return False

    def wait_boot(self, timeout=40):
        ok = self.wait_log('entering event loop', timeout)
        time.sleep(1.0)
        return ok

    # ---- keyboard ----
    def key(self, k, wait=0.06):
        self.cmd('sendkey ' + k, wait)

    def type(self, text):
        for ch in text:
            if ch in _PLAIN:
                self.key(_PLAIN[ch])
            elif ch in _SHIFTED:
                self.key('shift-' + _SHIFTED[ch])
            elif ch.isupper():
                self.key('shift-' + ch.lower())
            else:
                self.key(ch)

    # ---- mouse (PS/2 relative; GOS clamps to the screen, so a corner is used as the reference) ----
    scale = (1, 1)                                  # (num, den) GOS mouse speed: guest motion = raw * num / den

    def _rel(self, dx, dy, dz=0):
        n, d = self.scale
        if (n, d) != (1, 1):                           # compensate for the scale so the target (x,y) stays exact
            dx = (dx * d + (n // 2 if dx > 0 else -(n // 2))) // n if dx else 0
            dy = (dy * d + (n // 2 if dy > 0 else -(n // 2))) // n if dy else 0
        while dx or dy:
            sx = max(-60, min(60, dx)); sy = max(-60, min(60, dy))
            self.cmd('mouse_move %d %d' % (sx, sy), 0.012)
            dx -= sx; dy -= sy
        if dz:
            self.cmd('mouse_move 0 0 %d' % dz, 0.03)

    def home(self):
        self._rel(-SW - 100, -SH - 100)
        self.mx = self.my = 0

    def move(self, x, y):
        """Move the cursor to (x,y) on the GOS screen. Assumes a 1:1 mouse scale (default)."""
        x = max(0, min(SW - 1, x)); y = max(0, min(SH - 1, y))
        self._rel(x - self.mx, y - self.my)
        self.mx, self.my = x, y

    def down(self): self.cmd('mouse_button 1', 0.05)
    def up(self): self.cmd('mouse_button 0', 0.05)

    def click(self, x, y):
        self.move(x, y); self.down(); self.up()

    def dblclick(self, x, y, gap=0.05):
        self.move(x, y); self.down(); self.up(); time.sleep(gap); self.down(); self.up()

    def drag(self, x0, y0, x1, y1, steps=8):
        self.move(x0, y0); self.down()
        for i in range(1, steps + 1):
            self.move(x0 + (x1 - x0) * i // steps, y0 + (y1 - y0) * i // steps)
            time.sleep(0.04)
        time.sleep(0.1)
        self.up()

    def wheel(self, n, x=None, y=None):
        """n > 0 = wheel down. QEMU HMP: positive dz = wheel UP, so it is flipped here."""
        if x is not None:
            self.move(x, y)
        for _ in range(abs(n)):
            self.cmd('mouse_move 0 0 %d' % (-1 if n > 0 else 1), 0.05)

    # ---- screen ----
    def alive(self):
        return self.proc.poll() is None

    def why_dead(self):
        """Explain why QEMU died: exit code, QEMU stderr and the tail of the serial log."""
        try:
            err = open(os.path.join(self.dir, 'qemu.err'), 'rb').read().decode('latin1').strip()
        except OSError:
            err = ''
        return 'QEMU died (code %s). stderr: %s\nserial (tail): %s' % (self.proc.poll(), err[-300:] or '-', self.log()[-400:])

    def shot(self, path=None):
        """screendump -> return an Img (RGB pixels)."""
        if not self.alive():
            raise RuntimeError(self.why_dead())
        ppm = os.path.join(self.dir, 'shot%d.ppm' % self.nshot); self.nshot += 1
        self.cmd('screendump ' + ppm, 0.5)
        for _ in range(20):
            if os.path.exists(ppm) and os.path.getsize(ppm) > 100:
                break
            time.sleep(0.1)
        if not os.path.exists(ppm):
            raise RuntimeError('screendump produced no file. ' + (self.why_dead() if not self.alive() else 'QEMU is still alive (monitor stuck?); serial: ' + self.log()[-400:]))
        im = Img.load_ppm(ppm)
        if path:
            im.save_png(path)
        return im

    # ---- disk ----
    def vfs(self):
        return read_vfs(self.img)

    def save_to_disk(self):
        """Special > Save to Disk via the menu; close the dialog with Return."""
        self.menu_item('Special', 'Save to Disk')
        time.sleep(1.0)
        self.key('ret')
        time.sleep(0.3)

    def menu_item(self, menu_title, item_prefix):   # filled in by gui_test (needs menu coordinates); fallback: unavailable
        raise NotImplementedError

    def quit(self):
        try:
            self.cmd('quit', 0.1)
        except OSError:
            pass
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()

    def close(self, keep=False):
        self.quit()
        if not keep:
            shutil.rmtree(self.dir, ignore_errors=True)


class Img:
    def __init__(self, w, h, px):
        self.w, self.h, self.px = w, h, px           # px = bytes RGB

    @staticmethod
    def load_ppm(path):
        d = open(path, 'rb').read()
        parts, i = [], 0
        while len(parts) < 4:                         # P6 w h max
            while d[i:i + 1].isspace(): i += 1
            j = i
            while not d[j:j + 1].isspace(): j += 1
            parts.append(d[i:j]); i = j
        i += 1
        w, h = int(parts[1]), int(parts[2])
        return Img(w, h, d[i:i + w * h * 3])

    def get(self, x, y):
        o = (y * self.w + x) * 3
        return tuple(self.px[o:o + 3])

    def lum(self, x, y):
        r, g, b = self.get(x, y); return (r * 30 + g * 59 + b * 11) // 100

    def black(self, x, y): return self.lum(x, y) < 80
    def white(self, x, y): return self.lum(x, y) > 200

    def region_hash(self, x, y, w, h):
        import hashlib
        m = hashlib.sha1()
        for yy in range(y, min(self.h, y + h)):
            o = (yy * self.w + x) * 3
            m.update(self.px[o:o + w * 3])
        return m.hexdigest()

    def count(self, x, y, w, h, pred):
        n = 0
        for yy in range(y, min(self.h, y + h)):
            for xx in range(x, min(self.w, x + w)):
                if pred(self, xx, yy): n += 1
        return n

    def diff(self, other, box=None):
        """number of differing pixels (box = x,y,w,h)."""
        x, y, w, h = box or (0, 0, self.w, self.h)
        n = 0
        for yy in range(y, y + h):
            a = self.px[(yy * self.w + x) * 3:(yy * self.w + x + w) * 3]
            b = other.px[(yy * other.w + x) * 3:(yy * other.w + x + w) * 3]
            if a != b:
                for i in range(0, len(a), 3):
                    if a[i:i + 3] != b[i:i + 3]: n += 1
        return n

    def save_png(self, path):
        try:
            from PIL import Image
            Image.frombytes('RGB', (self.w, self.h), self.px).save(path)
        except Exception:
            open(path.replace('.png', '.ppm'), 'wb').write(b'P6 %d %d 255\n' % (self.w, self.h) + self.px)
