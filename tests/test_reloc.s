.equ char_H, 0x48
.equ char_e, 0x65
.equ char_l, 0x6C
.equ char_o, 0x6f
.equ char_W, 0x57
.equ char_r, 0x72
.equ char_d, 0x64
.equ char_space, 0x20
.equ char_exclamation, 0x21
.equ char_end, 0x0

.text

.global main
.extern writeln, offset

main:
    push &hello_world
    call $writeln
    test r0, r1
    jne $end
    mov r0, 4
end:
    halt

.section .rodata #, "a" flags are not necessary for this section name, will infer it from name

hello_world: .byte char_H, char_e, char_l, char_l, char_o, char_space, char_W, char_o, char_r, char_l, char_d, char_exclamation, char_end

.data

n:
.word 5, offset + 7 * 2 - (6 ^ 3), 3 * -6, ARRAY_BEGIN + 3, n

ARRAY_BEGIN:
.skip 100, 0xff
ARRAY_END:

.equ ARRAY_LENGTH, (ARRAY_END - ARRAY_BEGIN) / 2   # word array length
# .equ TEST_BAD, ARRAY_END * 3 - 75 & 0b101

.end