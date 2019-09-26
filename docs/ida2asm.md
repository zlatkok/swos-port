# ida2asm

ida2asm is a tool for converting pseudo-assembly output from IA Pro into
something that can actually be assembled without errors.

Currently only possible output format is MASM x86, but having multiple output
formats was an initial idea so it was designed to be easily augmented.

Aside from conversion, second task is symbol manipulation (importing,
exporting, removing, etc.). This is specified via a custom format text file.

## Command-line parameters

## Input file

## Performance

Parser was written with ultimate performance in mind . Tokenizer is

v1 in Python
horribly slow/we need to run often

### gen-lookup
2 goals
be as fast as possible
how:
parser 8 threads
cache friendly
~30ms
haxx & temp. fixes
