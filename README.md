# FernandoOS

Kernel Unix-like de 32 bits escrito desde cero en C y ensamblador x86.
Diseñado para ejecutarse en máquina virtual (QEMU).

## Características

- **Arranque**: GRUB Multiboot 1
- **Arquitectura**: x86 32-bit (modo protegido)
- **GDT**: tabla de descriptores globales (null, código, datos)
- **IDT**: tabla de descriptores de interrupciones (256 entradas)
- **ISR**: manejadores de las 32 excepciones x86 con kernel panic
- **IRQ**: controlador PIC 8259A remapeado (IRQ 0-15 → INT 32-47)
- **Timer**: PIT 8253 a 100 Hz
- **Teclado**: PS/2 con scancodes set 1, US QWERTY, shift/caps lock
- **VGA**: driver de texto 80×25, 16 colores, scroll automático
- **Memoria**: heap con bump allocator (4 MiB desde 0x400000)
- **Shell**: intérprete de comandos interactivo

## Requisitos

### Cross-compiler i686-elf-gcc

Es obligatorio un cross-compiler para no enlazar bibliotecas del host.

**Opción rápida (paquetes):**
```bash
# Arch Linux
yay -S cross-i686-elf-gcc cross-i686-elf-binutils

# macOS (Homebrew)
brew install i686-elf-gcc
```

**Compilar desde fuente (universal):**
```bash
# Instalar dependencias
sudo apt install build-essential bison flex libgmp3-dev \
     libmpc-dev libmpfr-dev texinfo

# Variables
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

# Binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.gz
tar xf binutils-2.41.tar.gz && mkdir binutils-build && cd binutils-build
../binutils-2.41/configure --target=$TARGET --prefix=$PREFIX \
    --with-sysroot --disable-nls --disable-werror
make -j$(nproc) && make install && cd ..

# GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
tar xf gcc-13.2.0.tar.gz && mkdir gcc-build && cd gcc-build
../gcc-13.2.0/configure --target=$TARGET --prefix=$PREFIX \
    --disable-nls --enable-languages=c --without-headers
make -j$(nproc) all-gcc all-target-libgcc
make install-gcc install-target-libgcc && cd ..
```

### Otras herramientas
```bash
# Debian/Ubuntu/Fedora
sudo apt install nasm grub-pc-bin grub-common xorriso qemu-system-x86
# Fedora
sudo dnf install nasm grub2-tools-extra xorriso qemu-system-x86
```

## Compilar y ejecutar

```bash
cd FernandoOS

# Compilar el kernel
make

# Crear ISO booteable
make iso

# Ejecutar en QEMU (terminal)
make run

# Ejecutar en QEMU (ventana gráfica)
make run-gui

# Depurar (log de interrupciones)
make run-debug
```

## Comandos de la shell

| Comando        | Descripción                        |
|----------------|------------------------------------|
| `help`         | Lista de comandos disponibles      |
| `clear`        | Limpiar pantalla                   |
| `echo <texto>` | Imprimir texto                     |
| `mem`          | Información de memoria del heap    |
| `uname`        | Información del sistema            |
| `uptime`       | Tiempo desde el arranque           |
| `history`      | Historial de comandos              |
| `colors`       | Paleta de 16 colores VGA           |
| `about`        | Acerca de FernandoOS               |
| `reboot`       | Reiniciar la máquina virtual       |
| `halt`         | Detener el sistema                 |

## Estructura del proyecto

```
FernandoOS/
├── Makefile
├── linker.ld          — Script del enlazador (kernel @ 1 MiB)
├── boot/
│   └── boot.asm       — Cabecera Multiboot + punto de entrada
├── kernel/
│   └── kernel.c       — Punto de entrada del kernel (kernel_main)
├── cpu/
│   ├── gdt.{h,c}      — Global Descriptor Table
│   ├── gdt_flush.asm  — Recarga segmentos tras instalar GDT
│   ├── idt.{h,c}      — Interrupt Descriptor Table
│   ├── idt_load.asm   — Instrucción LIDT
│   ├── isr.{h,c}      — Manejadores de excepciones (0-31)
│   ├── isr_stubs.asm  — Stubs en ensamblador para ISRs
│   ├── irq.{h,c}      — Manejadores de IRQs (0-15), remap PIC
│   ├── irq_stubs.asm  — Stubs en ensamblador para IRQs
│   └── ports.h        — Acceso a puertos I/O (inb/outb)
├── drivers/
│   ├── vga.{h,c}      — Driver de texto VGA 80×25
│   ├── timer.{h,c}    — Driver PIT (temporizador)
│   └── keyboard.{h,c} — Driver teclado PS/2
├── shell/
│   └── shell.{h,c}    — Shell interactiva
├── memory/
│   └── mem.{h,c}      — Heap (bump allocator, 4 MiB)
└── libc/
    └── string.{h,c}   — Funciones de cadena (sin libc estándar)
```

## Mapa de memoria

| Dirección     | Contenido                  |
|---------------|----------------------------|
| `0x00000000`  | IVT de modo real / BIOS    |
| `0x000A0000`  | Memoria de video VGA       |
| `0x000B8000`  | Buffer de texto VGA        |
| `0x00100000`  | Kernel (cargado por GRUB)  |
| `0x00400000`  | Heap del kernel (4 MiB)    |

## Próximos pasos sugeridos

- [ ] Paginación y memoria virtual
- [ ] Sistema de archivos (FAT12/ext2 básico)
- [ ] Modo usuario (ring 3) y llamadas al sistema
- [ ] Multitarea cooperativa / planificador round-robin
- [ ] Driver de disco ATA/IDE
- [ ] Puerto serie (UART 16550)
