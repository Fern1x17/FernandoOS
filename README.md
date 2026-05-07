# FernandoOS

Kernel Unix-like de 32 bits escrito desde cero en C y ensamblador x86.
Diseñado para ejecutarse en máquina virtual (QEMU).

## Características

- **Arranque**: GRUB Multiboot 1
- **Arquitectura**: x86 32-bit (modo protegido)
- **GDT**: tabla de descriptores globales
- **IDT**: tabla de descriptores de interrupciones (256 entradas)
- **ISR**: manejadores de las 32 excepciones x86 con kernel panic
- **IRQ**: controlador PIC 8259A remapeado (IRQ 0-15 → INT 32-47)
- **Timer**: PIT 8253 a 100 Hz
- **Teclado**: PS/2 con scancodes set 1, ES-QWERTY; tecla muerta ´ para vocales acentuadas; AltGr+q=æ, AltGr+vocal=acentuada
- **VGA**: driver de texto 80×25, 16 colores, scroll automático
- **Memoria**:
  - Heap con free-list doblemente enlazada + coalescing (`kmalloc`/`kfree` reales)
  - PMM: bitmap de frames físicos (hasta 32 MB)
  - Paginación: identity map 32 MB, manejador de page-fault
  - VMM: creación/destrucción/switch de page directories
- **Disco (ATA PIO)**: driver ATA primario master; lectura/escritura de sectores en PIO
- **Persistencia**: VFS y usuarios se guardan en `disk.img` en cada modificación y se restauran al arrancar; en primer arranque con disco virgen los valores por defecto se persisten automáticamente
- **Filesystem**: VFS en RAM (archivos, directorios, ls, cd, cat, write, rm…) — **persistente**
- **Scheduler**: round-robin preemptivo con context switch en ensamblador
- **Usuarios**: cuentas con login, contraseñas hasheadas (djb2), su, passwd — **persistentes**
- **Shell**: loop de login interactivo; prompt diferenciado (`#` root, `$` usuario)
- **Python**: intérprete Python mínimo integrado en el kernel — variables, if/elif/else, while, for/range, print/input, str/int/len/abs, operadores aritméticos y booleanos

## Requisitos

### Cross-compiler i686-elf-gcc

**Opción rápida (paquetes):**
```bash
# Arch Linux
yay -S cross-i686-elf-gcc cross-i686-elf-binutils

# macOS (Homebrew)
brew install i686-elf-gcc
```

**Compilar desde fuente:**
```bash
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

El Makefile busca el compiler en `/home/fernando/opt/cross/bin` por defecto.
Se puede cambiar con: `make CROSS=/otra/ruta/bin`

### Otras herramientas
```bash
# Fedora
sudo dnf install nasm grub2-tools-extra xorriso qemu-system-x86

# Debian/Ubuntu
sudo apt install nasm grub-pc-bin grub-common xorriso qemu-system-x86
```

## Compilar y ejecutar

```bash
cd FernandoOS

# Compilar kernel
make

# Crear ISO booteable
make iso

# Crear disco de datos (solo la primera vez — ¡no borrar entre arranques!)
make disk

# Ejecutar en QEMU con persistencia (terminal curses)
make run

# Ejecutar en QEMU con persistencia (ventana gráfica)
make run-gui

# Ejecutar sin disco (sin persistencia, rápido para desarrollo)
make run-nodisk

# Depurar
make run-debug

# Limpiar objetos e ISO (conserva disk.img)
make clean

# Borrar todo incluyendo el disco
make diskclean
```

> **Importante:** `disk.img` es el fichero que guarda todos los datos entre reinicios.
> No borrarlo salvo que quieras reiniciar el sistema desde cero (`make diskclean`).

## Login

Al arrancar el sistema pide usuario y contraseña.

| Usuario    | Contraseña   | UID  | Home              |
|------------|--------------|------|-------------------|
| `root`     | `root`       | 0    | `/root`           |
| `fernando` | `fernando`   | 1000 | `/home/fernando`  |

`root` muestra el prompt `#`. Los demás usuarios muestran `$`.

Los cambios de contraseña y los usuarios creados con `adduser` **persisten entre reinicios**.

## Persistencia

El kernel usa ATA PIO para leer y escribir directamente en `disk.img`:

| Región en disco | Contenido                    |
|-----------------|------------------------------|
| Sector 0        | Cabecera VFS (magic, versión)|
| Sectores 1–N    | Array de nodos VFS           |
| Sector 299      | Cabecera de usuarios         |
| Sectores 300–M  | Array de cuentas de usuario  |

Tras cada `mkdir`, `touch`, `write`, `rm`, `passwd`, `adduser` o `deluser`,
el kernel escribe automáticamente los datos actualizados al disco.
En el próximo arranque los recupera con `vfs_load()` y `users_load()`.

Si no se pasa `disk.img` a QEMU (target `run-nodisk`), el sistema funciona
igualmente pero sin persistencia.

## Comandos de la shell

### Sistema

| Comando          | Descripción                         |
|------------------|-------------------------------------|
| `help`           | Lista de comandos disponibles       |
| `clear`          | Limpiar pantalla                    |
| `echo <texto>`   | Imprimir texto                      |
| `uname`          | Información del sistema             |
| `uptime`         | Tiempo desde el arranque            |
| `history`        | Historial de comandos               |
| `colors`         | Paleta de 16 colores VGA            |
| `about`          | Acerca de FernandoOS                |
| `reboot`         | Reiniciar                           |
| `halt`           | Apagar                              |

### Memoria

| Comando | Descripción                                                   |
|---------|---------------------------------------------------------------|
| `mem`   | PMM (frames físicos), heap (bloques, mayor bloque libre)      |

### Filesystem (persistente)

| Comando            | Descripción                        |
|--------------------|------------------------------------|
| `ls [dir]`         | Listar directorio                  |
| `pwd`              | Ruta actual                        |
| `cd <dir>`         | Cambiar directorio                 |
| `mkdir <nombre>`   | Crear directorio *(se guarda)*     |
| `touch <nombre>`   | Crear fichero vacío *(se guarda)*  |
| `cat <fichero>`    | Mostrar contenido                  |
| `write <f> <txt>`  | Escribir texto en fichero *(se guarda)* |
| `rm <nombre>`      | Eliminar fichero/dir *(se guarda)* |

### Procesos

| Comando | Descripción               |
|---------|---------------------------|
| `ps`    | Tabla de procesos activos |

### Usuarios (persistentes)

| Comando                  | Descripción                                            |
|--------------------------|--------------------------------------------------------|
| `whoami`                 | Nombre, UID y home del usuario actual                  |
| `users`                  | Lista todos los usuarios del sistema                   |
| `passwd [usuario]`       | Cambiar contraseña *(se guarda)*                       |
| `adduser <nombre> <uid>` | Crear usuario — solo root *(se guarda)*                |
| `deluser <nombre>`       | Eliminar usuario — solo root *(se guarda)*             |
| `su <usuario>`           | Cambiar de usuario (root cambia sin contraseña)        |
| `logout`                 | Cerrar sesión y volver al prompt de login              |

## Estructura del proyecto

```
FernandoOS/
├── Makefile
├── disk.img               — Disco de datos (NO borrar entre ejecuciones)
├── linker.ld              — Script del enlazador (kernel @ 1 MiB)
├── boot/
│   └── boot.asm           — Cabecera Multiboot + punto de entrada
├── kernel/
│   └── kernel.c           — kernel_main: secuencia de arranque
├── cpu/
│   ├── gdt.{h,c}          — Global Descriptor Table
│   ├── gdt_flush.asm
│   ├── idt.{h,c}          — Interrupt Descriptor Table
│   ├── idt_load.asm
│   ├── isr.{h,c}          — Manejadores de excepciones (0–31)
│   ├── isr_stubs.asm
│   ├── irq.{h,c}          — Manejadores de IRQs, remap PIC
│   ├── irq_stubs.asm
│   └── ports.h            — Acceso a puertos I/O (inb/outb/inw/outw)
├── drivers/
│   ├── vga.{h,c}          — Driver de texto VGA 80×25
│   ├── timer.{h,c}        — Driver PIT 100 Hz
│   ├── keyboard.{h,c}     — Driver teclado PS/2
│   └── ata.{h,c}          — Driver ATA PIO (lectura/escritura de sectores)
├── memory/
│   ├── mem.{h,c}          — Heap: free-list con coalescing
│   ├── pmm.{h,c}          — Physical Memory Manager (bitmap)
│   ├── paging.{h,c}       — Identity map 32 MB, page-fault handler
│   └── vmm.{h,c}          — Virtual Memory Manager
├── fs/
│   └── vfs.{h,c}          — RAM filesystem + persistencia ATA
├── sched/
│   ├── sched.{h,c}        — Planificador round-robin
│   └── switch.asm         — Context switch en ensamblador
├── users/
│   └── users.{h,c}        — Sistema de usuarios + persistencia ATA
├── shell/
│   └── shell.{h,c}        — Shell interactiva con login y comandos
└── libc/
    └── string.{h,c}       — Funciones de cadena (sin libc estándar)
```

## Mapa de memoria

| Dirección      | Contenido                          |
|----------------|------------------------------------|
| `0x00000000`   | IVT de modo real / BIOS            |
| `0x000B8000`   | Buffer de texto VGA                |
| `0x00100000`   | Kernel (cargado por GRUB)          |
| `0x00400000`   | Heap del kernel (free-list, 4 MiB) |
| `0x00800000`   | Frames dinámicos (PMM)             |

## Mapa del disco (`disk.img`)

| Sector(es)  | Contenido                        |
|-------------|----------------------------------|
| 0           | Cabecera VFS (magic + versión)   |
| 1 – ~262    | Array de nodos VFS               |
| 299         | Cabecera de usuarios             |
| 300 – ~301  | Array de cuentas de usuario      |

## Teclado — caracteres especiales

| Combinación       | Carácter |
|-------------------|----------|
| ´ + a / e / i / o / u | á é í ó ú |
| ´ + A / E / I / O / U | Á É Í Ó Ú |
| AltGr + q         | æ        |
| AltGr + a         | á        |
| AltGr + e         | é        |
| AltGr + i         | í        |
| AltGr + o         | ó        |
| AltGr + u         | ú        |
| AltGr + 2         | @        |
| AltGr + 3         | #        |
| AltGr + 4         | ~        |
| AltGr + `         | [        |
| AltGr + +         | ]        |
| AltGr + ´         | {        |
| AltGr + ç         | }        |
| AltGr + -         | \\       |

> La tecla ´ es una **tecla muerta**: no produce carácter inmediatamente.
> La siguiente vocal recibe el acento agudo. Cualquier otra tecla emite ´ seguido del carácter normal.

## Python integrado

FernandoOS incluye un intérprete Python mínimo accesible desde la shell.

### Escribir un script

Usa `write` con `\n` para insertar saltos de línea:

```bash
touch hola.py
write hola.py nombre = input("Tu nombre: ")\nprint("Hola " + nombre + "!")
python hola.py
```

### Ejemplo: FizzBuzz

```bash
write fizz.py for i in range(1, 21):\n    if i % 15 == 0:\n        print("FizzBuzz")\n    elif i % 3 == 0:\n        print("Fizz")\n    elif i % 5 == 0:\n        print("Buzz")\n    else:\n        print(i)
python fizz.py
```

### Funcionalidades soportadas

| Característica | Ejemplo |
|----------------|---------|
| Variables int/str | `x = 42`, `s = "hola"` |
| Aritmética | `+ - * // %` |
| Comparaciones | `== != < > <= >=` |
| Booleanos | `and or not` |
| Condicionales | `if / elif / else` |
| Bucle while | `while x > 0:` |
| Bucle for | `for i in range(10):` |
| Control de flujo | `break continue pass` |
| Asignación aumentada | `x += 1` |
| Funciones integradas | `print() input() str() int() len() abs()` |
| Concatenación | `"Hola " + nombre` |

> **Límite de script**: los ficheros VFS tienen un máximo de 2048 bytes.
> Usa `\n` en `write` para insertar saltos de línea multi-línea.

## Próximos pasos sugeridos

- [ ] Llamadas al sistema (syscalls) vía interrupción software
- [ ] Modo usuario (ring 3) con espacios de dirección separados
- [ ] Sistema de archivos FAT12 real sobre el disco ATA
- [ ] Puerto serie UART 16550
- [ ] Shell con redirección (`>`, `|`) y variables de entorno
