global gdt_flush
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]

    mov ax, 0x10        ; Selector segmento de datos (entrada 2)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.flush     ; Far jump al selector de código (entrada 1)
.flush:
    ret
