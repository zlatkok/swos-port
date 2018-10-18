#!/bin/sh

files=(swos-*.asm)
for file in "${files[@]}"; do
    ml /coff /Cp /c /nologo /Zd /Zi /FR $file >$file.out 2>/dev/null &
done
wait
