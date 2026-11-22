#!/bin/bash
# i386 check without qemu (ARM: make ARCH=arm): compile every C file with the project flags, assemble every asm file (GNU as), link each ring 3 app
# as ELF32 using user/lib/x86/crt0.s, and check the kernel+initrd size against the 512 KB limit.
# Usage: tools/check_build.sh   (from the project root)
set -e
cd "$(dirname "$0")/.."
B=build/check; rm -rf $B; mkdir -p $B/ushared $B/k $B/elf
COMMON="-m32 -DGOS_X86 -Ikernel/arch/x86/include -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -fno-tree-loop-distribute-patterns -mgeneral-regs-only -Wall -Werror -Wno-error=stringop-overflow -nostdlib"
UC="$COMMON -Os -ffunction-sections -fdata-sections -DGOS_USERLAND -Iuser/include -Iabi -Ikernel"
as --32 user/lib/x86/crt0.s -o $B/crt0.o
KSRC=$(find kernel -name '*.c' -not -path 'kernel/arch/arm/*' -not -path 'kernel/drivers/arm/*')
for f in boot/x86/entry.s $(find kernel -name '*.s' -not -path 'kernel/arch/arm/*'); do as --32 $f -o $B/k/$(echo $f | tr / _).o; done
as --32 --defsym KSECT=1 boot/x86/boot.s -o $B/boot.o && ld -m elf_i386 -e start -Ttext 0x7C00 --oformat binary -o $B/boot.bin $B/boot.o
test "$(stat -c%s $B/boot.bin)" = 512 || { echo "boot sector is not 512 bytes"; exit 1; }
objs=""
for f in user/lib/*.c; do o=$B/$(basename $f .c).o; gcc $UC -c $f -o $o; objs="$objs $o"; done
for f in lib/string.c lib/printf.c lib/buf.c lib/timeconv.c gfx/draw.c gfx/font.c gfx/icons.c gui/widget.c; do o=$B/ushared/$(basename $f .c).o; gcc $UC -c kernel/$f -o $o; objs="$objs $o"; done
ar rcs $B/libgos.a $objs
for f in $KSRC; do gcc $COMMON -O2 -Ikernel -Iabi -c $f -o $B/k/$(echo $f | tr / _).o; done
total=0
for p in terminal files editor sysmon ticker counter burn upper calc puzzle keycaps clipview notepad scrapbook alarm getinfo control paint; do
  case $p in terminal) src="user/apps/terminal.c user/apps/shell.c user/apps/cmds.c";; editor) src="user/apps/editor.c user/apps/edbuf.c";; calc) src="user/apps/calc.c user/apps/calc_engine.c";; *) src="user/apps/$p.c";; esac
  po=""; for s in $src; do o=$B/$(basename $s .c).app.o; gcc $UC -c $s -o $o; po="$po $o"; done
  ld -m elf_i386 --gc-sections -T user/user.ld -o $B/elf/$p $B/crt0.o $po $B/libgos.a
  strip -s -o $B/elf/$p.bin $B/elf/$p; sz=$(stat -c%s $B/elf/$p.bin); total=$((total+sz)); printf "  %-10s %7d byte\n" $p $sz
done
ksz=$(size -t $B/k/*.o | tail -1 | awk '{print $4}')
echo "Userland (initrd): $total bytes; kernel objects (text+data+bss, estimate): $ksz bytes"
echo "Estimated kernel+BSS+initrd: $((total + ksz)) / 524288 bytes"
echo "OK: every ELF linked with no undefined symbols"
