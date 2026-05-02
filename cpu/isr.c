#include "isr.h"
#include "idt.h"
#include "../drivers/vga.h"
#include <stdint.h>

static isr_handler_t interrupt_handlers[256];

static const char* exception_names[] = {
    "Division por cero",
    "Debug",
    "Interrupcion no enmascarable",
    "Breakpoint",
    "Desbordamiento",
    "Limite de rango excedido",
    "Opcode invalido",
    "Dispositivo no disponible",
    "Doble fallo",
    "Segment overrun del coprocesador",
    "TSS invalido",
    "Segmento no presente",
    "Fallo de segmento de pila",
    "Fallo de proteccion general",
    "Fallo de pagina",
    "Reservado",
    "Error FPU x87",
    "Verificacion de alineacion",
    "Machine check",
    "Reservado", "Reservado", "Reservado", "Reservado",
    "Reservado", "Reservado", "Reservado", "Reservado",
    "Reservado", "Reservado", "Reservado", "Reservado",
    "Reservado"
};

void isr_install(void) {
    idt_set_gate(0,  (uint32_t)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (uint32_t)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x08, 0x8E);
}

void register_interrupt_handler(uint8_t n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

void isr_handler(registers_t* regs) {
    if (interrupt_handlers[regs->int_no]) {
        interrupt_handlers[regs->int_no](regs);
        return;
    }

    /* Kernel panic por excepción no manejada */
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_puts("\n\n  *** KERNEL PANIC ***  ");
    vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
    vga_puts("\n\n  Excepcion: ");
    if (regs->int_no < 32)
        vga_puts(exception_names[regs->int_no]);
    vga_puts("\n  INT="); vga_put_hex(regs->int_no);
    vga_puts("  ERR="); vga_put_hex(regs->err_code);
    vga_puts("  EIP="); vga_put_hex(regs->eip);
    vga_puts("\n  CS=");  vga_put_hex(regs->cs);
    vga_puts("  EFLAGS="); vga_put_hex(regs->eflags);
    vga_puts("\n\n  Sistema detenido.\n");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}
