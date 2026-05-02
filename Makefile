CC      = i686-elf-gcc
AS      = nasm
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Wno-unused-parameter
ASFLAGS = -f elf32

KERNEL  = FernandoOS.bin
ISO     = FernandoOS.iso

# Fuentes en orden: primero el boot, luego el resto
SRCS_ASM = \
	boot/boot.asm \
	cpu/gdt_flush.asm \
	cpu/idt_load.asm \
	cpu/isr_stubs.asm \
	cpu/irq_stubs.asm \
	sched/switch.asm

SRCS_C = \
	kernel/kernel.c \
	cpu/gdt.c \
	cpu/idt.c \
	cpu/isr.c \
	cpu/irq.c \
	drivers/vga.c \
	drivers/timer.c \
	drivers/keyboard.c \
	shell/shell.c \
	memory/mem.c \
	memory/pmm.c \
	memory/vmm.c \
	memory/paging.c \
	fs/vfs.c \
	sched/sched.c \
	libc/string.c

OBJS_ASM = $(SRCS_ASM:.asm=.o)
OBJS_C   = $(SRCS_C:.c=.o)
OBJS     = $(OBJS_ASM) $(OBJS_C)

.PHONY: all iso run run-debug clean help

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(CC) -T linker.ld -ffreestanding -O2 -nostdlib -o $@ $(OBJS) -lgcc
	@echo ""
	@echo "  Kernel compilado: $(KERNEL)"
	@ls -lh $(KERNEL)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# ── ISO booteable ────────────────────────────────────────────
iso: $(KERNEL)
	@mkdir -p isodir/boot/grub
	@cp $(KERNEL) isodir/boot/
	@printf 'set timeout=0\nset default=0\n\nmenuentry "FernandoOS v0.1.0" {\n    multiboot /boot/$(KERNEL)\n    boot\n}\n' \
		> isodir/boot/grub/grub.cfg
	$(shell which grub-mkrescue grub2-mkrescue 2>/dev/null | head -1) -o $(ISO) isodir 2>/dev/null
	@echo ""
	@echo "  ISO creada: $(ISO)"
	@ls -lh $(ISO)

# ── Ejecución en QEMU ────────────────────────────────────────
run: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M -display curses

run-gui: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M

run-debug: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M -d int,cpu_reset -no-reboot -no-shutdown

run-serial: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M \
		-serial stdio -display none

# ── Utilidades ───────────────────────────────────────────────
clean:
	@rm -f $(OBJS) $(KERNEL) $(ISO)
	@rm -rf isodir
	@echo "  Limpieza completa."

help:
	@echo ""
	@echo "  FernandoOS — Makefile"
	@echo "  ─────────────────────────────────────────"
	@echo "  make            Compilar kernel"
	@echo "  make iso        Crear imagen ISO booteable"
	@echo "  make run        Lanzar en QEMU (modo curses)"
	@echo "  make run-gui    Lanzar en QEMU (ventana)"
	@echo "  make run-debug  Lanzar con log de interrupciones"
	@echo "  make clean      Borrar archivos generados"
	@echo ""
	@echo "  Requisitos: i686-elf-gcc, nasm, grub-mkrescue, qemu-system-i386"
	@echo ""
