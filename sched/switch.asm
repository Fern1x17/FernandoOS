; switch_context(uint32_t* old_esp_ptr, uint32_t new_esp)
; Guarda el contexto actual (callee-saved regs + EBP), carga el nuevo.
; Convencion cdecl: argumentos en el stack, encima del return address.

global switch_context

switch_context:
    ; Guardar registros callee-saved del contexto saliente
    push ebp
    push ebx
    push esi
    push edi

    ; Ahora el stack tiene: edi, esi, ebx, ebp, ret_addr, old_esp_ptr, new_esp
    ; Offset desde ESP (despues de los 4 pushes de 4 bytes = 16 bytes):
    ;   [esp+16] = direccion de retorno de switch_context
    ;   [esp+20] = old_esp_ptr
    ;   [esp+24] = new_esp

    mov eax, [esp+20]   ; eax = puntero donde guardar el ESP antiguo
    mov [eax], esp      ; guardar ESP actual en *old_esp_ptr

    mov eax, [esp+24]   ; leer new_esp del stack VIEJO (antes de cambiar)
    mov esp, eax        ; cambiar al stack de la nueva tarea

    ; Restaurar registros callee-saved del contexto entrante
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
