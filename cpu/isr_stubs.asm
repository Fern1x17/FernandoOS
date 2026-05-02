; Macros para generar stubs de ISR
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0        ; error code ficticio
    push dword %1       ; número de interrupción
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1       ; número de interrupción (error code ya está en pila)
    jmp isr_common_stub
%endmacro

; Excepciones x86 (0-31)
ISR_NOERRCODE 0   ; Division by zero
ISR_NOERRCODE 1   ; Debug
ISR_NOERRCODE 2   ; Non-maskable interrupt
ISR_NOERRCODE 3   ; Breakpoint
ISR_NOERRCODE 4   ; Overflow
ISR_NOERRCODE 5   ; Bound range exceeded
ISR_NOERRCODE 6   ; Invalid opcode
ISR_NOERRCODE 7   ; Device not available
ISR_ERRCODE   8   ; Double fault
ISR_NOERRCODE 9   ; Coprocessor segment overrun
ISR_ERRCODE   10  ; Invalid TSS
ISR_ERRCODE   11  ; Segment not present
ISR_ERRCODE   12  ; Stack-segment fault
ISR_ERRCODE   13  ; General protection fault
ISR_ERRCODE   14  ; Page fault
ISR_NOERRCODE 15
ISR_NOERRCODE 16  ; x87 FPU error
ISR_ERRCODE   17  ; Alignment check
ISR_NOERRCODE 18  ; Machine check
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

extern isr_handler

isr_common_stub:
    pusha                   ; Guarda EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI

    mov ax, ds
    push eax                ; Guarda segmento de datos

    mov ax, 0x10            ; Carga segmento de datos del kernel
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp                ; Pasa puntero a registers_t como argumento
    call isr_handler
    add esp, 4

    pop eax                 ; Restaura segmento de datos
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8              ; Limpia int_no y err_code
    sti
    iret
