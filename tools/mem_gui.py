#!/usr/bin/env python3
"""mem_gui: boot at each RAM size, open every v0.6 feature (Files, Terminal, Paint, 7 DAs), do Trash/Put Away,
then report: did it boot, how many windows opened, any process killed / PANIC?  Usage: tools/mem_gui.py [os.img] [MB ...]"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gosvm import GosVM
HERE = os.path.dirname(os.path.abspath(__file__))
img = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1].endswith('.img') else os.path.join(HERE, '..', 'os.img')
sizes = [int(a) for a in sys.argv[1:] if a.isdigit()] or [3, 4, 8, 16, 64]
APPLE = ['About GOS...', '--', 'Alarm Clock', 'Calculator', 'Control Panel', 'Key Caps', 'Note Pad', 'Puzzle',
         'Scrapbook', '--', 'Terminal', 'File Manager', 'Text Editor', 'Paint', 'System Monitor']
def pick(vm, idx):
    vm.click(20, 12); time.sleep(0.25)
    y = 26
    for k, it in enumerate(APPLE):
        h = 8 if it == '--' else 20
        if k == idx: vm.click(24, y + 10); time.sleep(1.0); return
        y += h
print('%-4s %-6s %-9s %s' % ('-m', 'boot', 'windows', 'notes'))
for mb in sizes:
    vm = GosVM(img, mem=mb, tag='mem%d' % mb)
    ok = vm.wait_boot(40); note = ''
    n = 0
    if ok:
        vm.home()
        order = [11, 10, 13, 2, 3, 4, 5, 6, 7, 8]          # Files, Terminal, Paint, then 7 DAs
        for i in order:
            pick(vm, i); n += 1
            if 'PANIC' in vm.log(): break
        # Trash: select an item in the Files window and press Del
        lg = vm.log()
        killed = [l for l in lg.split('\n') if 'killed' in l]
        if 'PANIC' in lg: note = 'PANIC'
        elif killed: note = 'process killed: ' + killed[0][:60]
        else: note = 'no PANIC/kill'
        mem = [l for l in lg.split('\n') if l.startswith('mem:')]
        if mem: note += ' | ' + mem[0][:60]
    else:
        lg = vm.log(); note = 'PANIC' if 'PANIC' in lg else 'did not boot'
        note += ' | ' + (lg.strip().split('\n')[-1][:80] if lg.strip() else '')
    print('%-4d %-6s %-9s %s' % (mb, 'OK' if ok else 'FAIL', '%d/10' % n if ok else '-', note))
    sys.stdout.flush()
    vm.close()
