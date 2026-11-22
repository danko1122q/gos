#!/bin/sh
# Usage: tools/mkinitrd.sh <x86|arm> out.s prog1 prog2 ...   (prog = stripped ELF file in build/user/bin/<name>)
# Output: assembler source that embeds each program with .incbin (GNU as; clang accepts the same syntax)
arch="$1"; out="$2"; shift 2
case "$arch" in
  x86) hdr=".code32"; typ="@progbits" ;;
  arm) hdr=".arm";    typ="%progbits" ;;       # on ARM "@" starts a comment, so the section type is written as %progbits
  *) echo "mkinitrd: unknown architecture: $arch" >&2; exit 2 ;;
esac
{
  echo "$hdr"
  echo ".section .rodata"
  echo ".balign 4"
  echo ".globl initrd_count"
  echo "initrd_count: .long $#"
  echo ".globl initrd_table"
  echo "initrd_table:"
  i=0
  for f in "$@"; do echo "    .long name_$i, data_$i, end_$i - data_$i"; i=$((i+1)); done
  i=0
  for f in "$@"; do
    n=$(basename "$f")
    echo "name_$i: .asciz \"$n\""
    echo ".balign 4"
    echo "data_$i: .incbin \"$f\""
    echo "end_$i:"
    i=$((i+1))
  done
  echo ".section .note.GNU-stack,\"\",$typ"
} > "$out"
