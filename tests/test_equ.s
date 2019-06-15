.global main
.extern a, b, c

.equ c, a
.equ d, b + 2
.equ e, c + 7
.equ f, (arr_end - arr_begin) * 2 + (5 & 1)
# .equ g, a - b This is an invalid expression because both symbols a and b are undefined!
#               Expressions of this kind (a - b) are only allowed for symbols defined
#               in the same section (ex: arr_end - arr_begin as seen above).
.equ loop, main + 9 # This is a relative .equ symbol
# .equ x, y + 4 Circular references are detected and error is shown. The way this works is
# .equ y, x - 4 It loops through unevaluated expressions while any can be evaluated. As soon
#               as none are evaluated in one round, it exits and checks if all have been
#               evaluated. If equ_uneval_vect is NOT empty then show an error.

.data
data_entry:
.skip 50, 0x0f
arr_begin:
.skip 100
arr_end:

.text
main:
    push &c
start:
    add r0, &d
    sub r1, &e # Symbol 'loop' defines the start of this line
    mov r2, &f
    cmp r1, 0
    jgt $loop
    jmp $start
    jmp $.data
    ret