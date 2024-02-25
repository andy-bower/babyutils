-- SPDX-License-Identifier: MIT
-- (c) Copyright 2024 Andrew Bower
--
-- Subroutine macros

; Utility macros
;;;;;;;;;;;;;;;;

add2 macro x, y          ; x := x + y
    ldn x
    sub y
    sto x
    neg x
    endm

sub2 macro x, y          ; x := x - y
    ld x
    sub y
    sto x
    endm

inc macro x              ; x := x + 1
    add2 x, _one
    endm

dec macro x              ; x := x + 1
    sub2 x, _one
    endm

neg macro x              ; x := -x
    ldn x
    sto x
    endm

negi macro               ; AC := -AC
    sto _tmp
    ldn _tmp
    endm

ld  macro x
    ldn x
    negi
    endm
 
jsr macro x
    sto _tmp             ; save AC
    dec _sp
    ldn next_j
    sub push_instr_templ
    negi
    sto push_instr
push_instr:
    sto 0                ; placeholder for push instruction
    ld _tmp              ; restore AC as parameter
    jmp sub_j
push_instr_templ:
    sto 0                ; template for code gen
sub_j:
    eja x
next_j:
    eja next
next:
    endm

rts macro
    ld _sp
    sto ret_instr
    inc _sp
ret_instr:
    jmp 0                ; rely on encoding of JMP instruction
    endm

; Application
;;;;;;;;;;;;;

count: num 5

01:
_start:
    ld count
    jsr count_recursive
    hlt

count_recursive:
    sub one
    skn
    jst count_recursive
    rts

28:
_stack:
_sp:
    num _stack           ; full descending stack
_one:
    num 1
_tmp:
    num 0
