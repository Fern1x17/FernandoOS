CROSS   ?= /home/fernando/opt/cross/bin
CC       = $(CROSS)/i686-elf-gcc
AS       = nasm
CFLAGS  = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Wno-unused-parameter
ASFLAGS = -f elf32

KERNEL  = FernandoOS.bin
ISO     = FernandoOS.iso
DISK    = disk.img
DISK_MB = 4   # Tamaño del disco en MiB

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
	drivers/ata.c \
	shell/shell.c \
	memory/mem.c \
	memory/pmm.c \
	memory/vmm.c \
	memory/paging.c \
	fs/vfs.c \
	sched/sched.c \
	users/users.c \
	python/python.c \
	libc/string.c

OBJS_ASM = $(SRCS_ASM:.asm=.o)
OBJS_C   = $(SRCS_C:.c=.o)
OBJS     = $(OBJS_ASM) $(OBJS_C)

.PHONY: all iso disk run run-gui run-debug run-nodisk clean diskclean help

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

# ── ISO booteable ─────────────────────────────────────────────
iso: $(KERNEL)
	@mkdir -p isodir/boot/grub
	@cp $(KERNEL) isodir/boot/
	@printf 'set timeout=0\nset default=0\n\nmenuentry "FernandoOS v0.3.0" {\n    multiboot /boot/$(KERNEL)\n    boot\n}\n' \
		> isodir/boot/grub/grub.cfg
	$(shell which grub-mkrescue grub2-mkrescue 2>/dev/null | head -1) -o $(ISO) isodir 2>/dev/null
	@echo ""
	@echo "  ISO creada: $(ISO)"
	@ls -lh $(ISO)

# ── Disco de datos (se crea solo la primera vez) ──────────────
$(DISK):
	@dd if=/dev/zero of=$(DISK) bs=1M count=$(DISK_MB) 2>/dev/null
	@echo "  Disco creado: $(DISK) ($(DISK_MB) MiB)"

disk: $(DISK)

# ── Ejecución en QEMU ─────────────────────────────────────────
# -boot order=d  → arrancar desde CD-ROM (ISO con GRUB)
# -hda $(DISK)   → disco de datos en ATA primario master (0x1F0)

QEMU_BASE = qemu-system-i386 -cdrom $(ISO) -hda $(DISK) -m 32M -boot order=d

run: iso $(DISK)
	$(QEMU_BASE) -display curses

run-gui: iso $(DISK)
	$(QEMU_BASE)

run-debug: iso $(DISK)
	$(QEMU_BASE) -display curses -d int,cpu_reset -no-reboot -no-shutdown

# Ejecutar sin disco (sin persistencia, útil para desarrollo)
run-nodisk: iso
	qemu-system-i386 -cdrom $(ISO) -m 32M -boot order=d -display curses

# ── Limpieza ──────────────────────────────────────────────────
clean:
	@rm -f $(OBJS) $(KERNEL) $(ISO)
	@rm -rf isodir
	@echo "  Limpieza completa (el disco $(DISK) no se ha borrado)."

# Borra también el disco — ¡pierde todos los datos guardados!
diskclean: clean
	@rm -f $(DISK)
	@echo "  Disco borrado."

help:
	@echo ""
	@echo "  FernandoOS — Makefile"
	@echo "  ─────────────────────────────────────────"
	@echo "  make              Compilar kernel"
	@echo "  make iso          Crear imagen ISO booteable"
	@echo "  make disk         Crear disco de datos ($(DISK_MB) MiB)"
	@echo "  make run          Lanzar en QEMU curses (con disco)"
	@echo "  make run-gui      Lanzar en QEMU ventana (con disco)"
	@echo "  make run-nodisk   Lanzar sin disco (sin persistencia)"
	@echo "  make run-debug    Lanzar con log de interrupciones"
	@echo "  make clean        Borrar objetos e ISO (conserva disco)"
	@echo "  make diskclean    Borrar todo incluido el disco"
	@echo ""
	@echo "  Requisitos: i686-elf-gcc, nasm, grub-mkrescue, qemu-system-i386"
	@echo ""
