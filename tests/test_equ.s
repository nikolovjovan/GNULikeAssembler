.global main
.extern a, b

.equ c, a
.equ d, b + 2
.equ e, c + 7
.equ f, (arr_end - arr_begin) * 2 + (5 & 1)
# .equ g, a - b This is an invalid expression because both symbols a and b are undefined!
#               Expressions of this kind (a - b) are only allowed for symbols defined
#               in the same section (ex: arr_end - arr_begin as seen above).

.data
.skip 50, 0x0f
arr_begin:
.skip 100
arr_end:

.text
main:
    push &c
    add r0, &d
    sub r1, &e
    mov r1, &f
    ret