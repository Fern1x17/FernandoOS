; Constantes multiboot
MBALIGN  equ 1 << 0
MEMINFO  equ 1 << 1
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384      ; 16 KiB de pila del kernel
stack_top:

section .text
global _start
_start:
    mov esp, stack_top

    push 0          ; Limpiar EFLAGS
    popf

    push ebx        ; Puntero a info multiboot
    push eax        ; Magic de multiboot

    extern kernel_main
    call kernel_main

    ; Si kernel_main retorna, detener CPU
    cli
.hang:
    hlt
    jmp .hang
