; fictive file - this is a readable version of the code inserted into SWOS
; in short - load loader.bin into pitchDatBuffer and call it
; very strange - if I add the executable attribute to the data section (which
; seems logical so I can execute my code loaded into pitchDatBuffer), SWOS
; crashes when accessing the allocated memory. This seems to be a bug in the
; silly DOS4GW extender, while the code executes in the data segment without
; any problems (and without the executable attribute)

bits 32

pitchDatBuffer   equ 0x5535e
LoadFile         equ 0xa1a8
SWOS             equ 0x5758
stack_top        equ 0xb16eb
code_base        equ 0
data_base        equ 0

tmp01 equ 0x3146d
tmp02 equ 0x31471
tmp03 equ 0x31475
tmp04 equ 0x31479
tmp05 equ 0x3147d
tmp06 equ 0x31481
tmp07 equ 0x31485
tmp08 equ 0x31489
tmp09 equ 0x3148d
tmp10 equ 0x31491
tmp11 equ 0x31495
tmp12 equ 0x31499
tmp13 equ 0x3149d
tmp14 equ 0x314a1
tmp15 equ 0x314a5

; insert at 0x54f4, over "SAVE DISK FILING"
filename:
    db "loader.bin"

; patch the DumpTimerVariables function at 0xa929
start:
    ;int  1                          ; for debugging
    nop
    nop
    mov  [stack_top], esp           ; do this immediately in case
                                    ; loader.bin is not found
    pushfd
    pushad                          ; preserve all registers and flags
    mov  ebx, data_base             ; ebx will be the base register for accessing
                                    ; variables, to save on fixup records
    push dword [ebx + tmp01]
    push dword [ebx + tmp02]
    push dword [ebx + tmp09]        ; preserve all used pseudo-registers too,
    push dword [ebx + tmp10]        ; just in case
    lea  eax, [ebx + filename]
    mov  [ebx + tmp09], eax         ; tmp09 -> filename
    lea  eax, [ebx + pitchDatBuffer]
    mov  [ebx + tmp10], eax         ; tmp10 -> buffer (pitchDatBuffer)
                                    ; pitchDatBuffer was chosen because it is
                                    ; initialized to zero before every match
    call LoadFile                   ; if the file is not found, the function
                                    ; terminates the program
    mov  eax, [ebx + tmp02]         ; tmp02 = file length
    cmp  eax, 10032                 ; size of pitchDatBuffer
    jbe  .size_ok
.endless_loop:
    int  3
    jmp  short .endless_loop        ; their system
.size_ok:
    mov  ecx, ebx
    mov  eax, ebx                   ; eax = data relocation base
    mov  ebx, code_base             ; ebx = code relocation base
    add  ecx, pitchDatBuffer
    ;int  1                          ; for debugging
    nop
    nop
    call ecx                        ; start the loader
    pop  dword [tmp10]
    pop  dword [tmp09]
    pop  dword [tmp02]
    pop  dword [tmp01]
    popad
    popfd                           ; everything is clean now
    ;int 1
    nop
    nop
    jmp  SWOS                       ; continue normal execution
    retn
    retn
    retn                            ; just padding up to the next
    retn                            ; instruction
