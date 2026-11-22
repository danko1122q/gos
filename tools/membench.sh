#!/bin/sh
# Usage: tools/membench.sh build/bench/os.img [list of -m ...]   (run through 'make membench', which then calls this script)
# For each RAM size: boot QEMU headless for ~12 seconds, record boot OK/PANIC, the "mem:" line and the free frames (membench:).
IMG="$1"; shift
SIZES="${*:-4 6 8 12 16 24 32 64}"
QEMU="${QEMU:-qemu-system-i386}"
printf '%-5s %-7s %s\n' "-m" "result" "detail"
for m in $SIZES; do
    L=/tmp/membench_$m.log; rm -f "$L"; cp "$IMG" /tmp/membench.img
    timeout 12 "$QEMU" -m "$m" -drive format=raw,file=/tmp/membench.img -vga std -display none -serial file:"$L" -no-reboot >/dev/null 2>&1 || true
    if grep -aq "PANIC" "$L" 2>/dev/null; then r=PANIC
    elif grep -aq "entering event loop" "$L" 2>/dev/null; then r=OK
    else r=FAIL; fi
    printf '%-5s %-7s\n' "$m" "$r"
    grep -a "PANIC\|^mem:\|GOS: init ok\|membench:" "$L" 2>/dev/null | tr -d '\r' | sed 's/^/        /'
done
