; ============================================================
; CreateOS 0.0.1
; ============================================================
; ============================================================
;  context.asm — NASM 64-bit context switch
; ============================================================

global context_switch

section .text
bits 64

; ------------------------------------------------------------
; void context_switch(uint64_t* old_ctx, uint64_t* new_ctx)
; old_ctx -> pointeur vers un tableau de 16 registres
; new_ctx -> idem
; ------------------------------------------------------------
; Layout (16 x 8 bytes = 128 bytes) :
;   0  : r15
;   8  : r14
;   16 : r13
;   24 : r12
;   32 : r11
;   40 : r10
;   48 : r9
;   56 : r8
;   64 : rsi
;   72 : rdi
;   80 : rbp
;   88 : rdx
;   96 : rcx
;   104: rbx
;   112: rax
;   120: rip (sauvegardé via [rsp])
; ------------------------------------------------------------

context_switch:
    ; arguments :
    ; rdi = old_ctx
    ; rsi = new_ctx

    ; -------------------------
    ; Sauvegarde du contexte
    ; -------------------------
    mov [rdi + 0],  r15
    mov [rdi + 8],  r14
    mov [rdi + 16], r13
    mov [rdi + 24], r12
    mov [rdi + 32], r11
    mov [rdi + 40], r10
    mov [rdi + 48], r9
    mov [rdi + 56], r8
    mov [rdi + 64], rsi
    mov [rdi + 72], rdi
    mov [rdi + 80], rbp
    mov [rdi + 88], rdx
    mov [rdi + 96], rcx
    mov [rdi + 104], rbx
    mov [rdi + 112], rax

    ; sauvegarde RIP via la pile
    mov rax, [rsp]
    mov [rdi + 120], rax

    ; -------------------------
    ; Restauration du contexte
    ; -------------------------
    mov r15, [rsi + 0]
    mov r14, [rsi + 8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov r11, [rsi + 32]
    mov r10, [rsi + 40]
    mov r9,  [rsi + 48]
    mov r8,  [rsi + 56]
    mov rsi, [rsi + 64]
    mov rdi, [rsi + 72]
    mov rbp, [rsi + 80]
    mov rdx, [rsi + 88]
    mov rcx, [rsi + 96]
    mov rbx, [rsi + 104]
    mov rax, [rsi + 112]

    ; restauration RIP
    mov rdx, [rsi + 120]
    mov [rsp], rdx

    ret
