#include "shell.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../memory/mem.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../cpu/ports.h"
#include "../libc/string.h"
#include "../fs/vfs.h"
#include "../sched/sched.h"
#include <stdint.h>

#define MAX_INPUT   256
#define MAX_ARGS    16
#define HISTORY_LEN 8

static char history[HISTORY_LEN][MAX_INPUT];
static int  hist_count = 0;

/* ── helpers ─────────────────────────────────────────────── */

static void print_separator(void) {
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts("────────────────────────────────────────\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void readline(char* buf, int max) {
    int i = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n' || c == '\r') {
            buf[i] = '\0';
            return;
        }
        if (c == '\b') {
            if (i > 0) { i--; vga_putchar('\b'); }
            continue;
        }
        if (c == 3) {          /* Ctrl+C */
            buf[0] = '\0';
            vga_puts("^C\n");
            return;
        }
        if (i < max - 1) {
            buf[i++] = c;
            vga_putchar(c);
        }
    }
}

static int split_args(char* line, char** argv) {
    int argc = 0;
    char* tok = strtok(line, " \t");
    while (tok && argc < MAX_ARGS - 1) {
        argv[argc++] = tok;
        tok = strtok((char*)0, " \t");
    }
    argv[argc] = (char*)0;
    return argc;
}

/* ── comandos existentes ──────────────────────────────────── */

static void cmd_help(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Comandos disponibles:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  help           - Muestra esta ayuda\n");
    vga_puts("  clear          - Limpia la pantalla\n");
    vga_puts("  echo <texto>   - Imprime texto\n");
    vga_puts("  mem            - Informacion de memoria\n");
    vga_puts("  uname          - Informacion del sistema\n");
    vga_puts("  uptime         - Tiempo desde arranque\n");
    vga_puts("  history        - Historial de comandos\n");
    vga_puts("  colors         - Muestra paleta de colores\n");
    vga_puts("  about          - Acerca de FernandoOS\n");
    vga_puts("  reboot         - Reiniciar sistema\n");
    vga_puts("  halt           - Apagar sistema\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Filesystem:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  ls [dir]       - Listar directorio\n");
    vga_puts("  pwd            - Ruta actual\n");
    vga_puts("  cd <dir>       - Cambiar directorio\n");
    vga_puts("  mkdir <nombre> - Crear directorio\n");
    vga_puts("  touch <nombre> - Crear fichero vacio\n");
    vga_puts("  cat <fichero>  - Mostrar contenido\n");
    vga_puts("  write <f> <t>  - Escribir texto en fichero\n");
    vga_puts("  rm <nombre>    - Eliminar fichero/dir\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Procesos:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  ps             - Tabla de procesos\n");
}

static void cmd_clear(void) {
    vga_clear();
}

static void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) vga_putchar(' ');
        vga_puts(argv[i]);
    }
    vga_putchar('\n');
}

static void cmd_mem(void) {
    /* ── PMM: memoria física ─────────────────────────────── */
    uint32_t ptotal = pmm_get_total();
    uint32_t pused  = pmm_get_used();
    uint32_t pfree  = pmm_get_free();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Memoria fisica (PMM):\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Total  : ");
    vga_put_dec(ptotal * PAGE_SIZE / 1024); vga_puts(" KiB  (");
    vga_put_dec(ptotal); vga_puts(" frames)\n");
    vga_puts("  Kernel : ");
    vga_put_dec(PMM_KERN_FRAMES * PAGE_SIZE / 1024);
    vga_puts(" KiB  reservados (0x00000000-0x007FFFFF)\n");
    vga_puts("  Usado  : ");
    vga_put_dec(pused * PAGE_SIZE / 1024); vga_puts(" KiB  (");
    vga_put_dec(pused); vga_puts(" frames)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("  Libre  : ");
    vga_put_dec(pfree * PAGE_SIZE / 1024); vga_puts(" KiB  (");
    vga_put_dec(pfree); vga_puts(" frames)\n");

    /* ── Heap kmalloc ────────────────────────────────────── */
    uint32_t hused  = mem_get_used();
    uint32_t hfree  = mem_get_free();
    uint32_t htotal = mem_get_total();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Heap kmalloc (bump allocator):\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Rango  : 0x00400000 - 0x007FFFFF\n");
    vga_puts("  Total  : "); vga_put_dec(htotal / 1024); vga_puts(" KiB\n");
    vga_puts("  Usado  : "); vga_put_dec(hused  / 1024); vga_puts(" KiB\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("  Libre  : "); vga_put_dec(hfree  / 1024); vga_puts(" KiB\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void cmd_uname(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("FernandoOS");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(" v0.1.0  i686  #1  (kernel freestanding)\n");
    vga_puts("Arquitectura: x86 32-bit  |  Modo: protegido\n");
    vga_puts("Boot:         GRUB Multiboot  |  VM: QEMU\n");
}

static void cmd_uptime(void) {
    uint32_t ticks = timer_get_ticks();
    uint32_t secs  = ticks / 100;
    uint32_t mins  = secs / 60;
    uint32_t hrs   = mins / 60;
    mins %= 60; secs %= 60;
    vga_puts("Tiempo de actividad: ");
    if (hrs) { vga_put_dec(hrs); vga_puts("h "); }
    vga_put_dec(mins); vga_puts("m ");
    vga_put_dec(secs); vga_puts("s  (ticks="); vga_put_dec(ticks); vga_puts(")\n");
}

static void cmd_history(void) {
    if (hist_count == 0) { vga_puts("Sin historial.\n"); return; }
    int start = hist_count > HISTORY_LEN ? hist_count - HISTORY_LEN : 0;
    for (int i = start; i < hist_count; i++) {
        int idx = i % HISTORY_LEN;
        vga_puts("  "); vga_put_dec((uint32_t)(i + 1)); vga_puts("  ");
        vga_puts(history[idx]); vga_putchar('\n');
    }
}

static void cmd_colors(void) {
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("Paleta VGA (16 colores):\n");
    for (int i = 0; i < 16; i++) {
        vga_set_color((vga_color_t)i, VGA_COLOR_BLACK);
        vga_puts("  Color ");
        vga_put_dec((uint32_t)i);
        vga_puts("  ABCDEF  ");
        vga_set_color(VGA_COLOR_BLACK, (vga_color_t)i);
        vga_puts("  ABCDEF  ");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_putchar('\n');
    }
}

static void cmd_about(void) {
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_puts("  ___                               _      ___  ____\n");
    vga_puts(" | __| ___  _ _  _ _   __ _ _ _  __| |___ / _ \\/ ___|\n");
    vga_puts(" | _| / -_)| '_|| ' \\ / _` | ' \\/ _` / _ \\ |  _\\__ \\\n");
    vga_puts(" |_|  \\___||_|  |_||_|\\__,_|_||_\\__,_\\___/\\____/___/\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("\n  Un sistema operativo Unix-like hecho desde cero.\n");
    vga_puts("  Arquitectura: x86 32-bit (modo protegido)\n");
    vga_puts("  Creado por:   Fernando\n");
    vga_puts("  Version:      0.1.0\n");
    vga_puts("  Licencia:     MIT\n");
}

static void cmd_reboot(void) {
    vga_puts("Reiniciando...\n");
    timer_sleep(500);
    /* Reset via controlador PS/2 */
    uint8_t tmp;
    do { tmp = inb(0x64); } while (tmp & 0x02);
    outb(0x64, 0xFE);
    __asm__ volatile("cli; hlt");
}

static void cmd_halt(void) {
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts("Sistema detenido. Puede apagar la maquina virtual.\n");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

/* ── nuevos comandos VFS ──────────────────────────────────── */

static void cmd_ls(int argc, char** argv) {
    if (argc < 2)
        vfs_ls(".");
    else
        vfs_ls(argv[1]);
}

static void cmd_pwd(void) {
    vfs_pwd();
}

static void cmd_cd(int argc, char** argv) {
    if (argc < 2)
        vfs_cd("/");
    else
        vfs_cd(argv[1]);
}

static void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        vga_puts("mkdir: falta el nombre\n");
        return;
    }
    if (vfs_mkdir(argv[1]) < 0)
        vga_puts("mkdir: error al crear el directorio\n");
}

static void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        vga_puts("touch: falta el nombre\n");
        return;
    }
    vfs_create(argv[1]);
}

static void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        vga_puts("cat: falta el nombre del fichero\n");
        return;
    }
    vfs_cat(argv[1]);
}

static void cmd_write(int argc, char** argv) {
    if (argc < 3) {
        vga_puts("write: uso: write <fichero> <texto...>\n");
        return;
    }
    /* Construir el texto desde argv[2] en adelante */
    char buf[VFS_MAX_DATA];
    int pos = 0;
    for (int i = 2; i < argc && pos < (int)(VFS_MAX_DATA - 1); i++) {
        if (i > 2 && pos < (int)(VFS_MAX_DATA - 2)) {
            buf[pos++] = ' ';
        }
        const char* src = argv[i];
        while (*src && pos < (int)(VFS_MAX_DATA - 1))
            buf[pos++] = *src++;
    }
    buf[pos] = '\0';

    if (vfs_write(argv[1], buf, (uint32_t)pos) < 0)
        vga_puts("write: error al escribir\n");
}

static void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        vga_puts("rm: falta el nombre\n");
        return;
    }
    vfs_rm(argv[1]);
}

/* ── comando ps ──────────────────────────────────────────── */

static void cmd_ps(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("PID  ESTADO    TICKS      NOMBRE\n");
    vga_puts("---  --------  ---------  --------\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    for (int i = 0; i < MAX_TASKS; i++) {
        task_t* t = sched_get_task(i);
        if (!t || t->state == TASK_DEAD) continue;

        /* PID */
        vga_put_dec(t->pid);
        vga_puts("    ");

        /* Estado */
        switch (t->state) {
            case TASK_RUNNING:
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                vga_puts("Running  ");
                break;
            case TASK_READY:
                vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                vga_puts("Ready    ");
                break;
            case TASK_BLOCKED:
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                vga_puts("Blocked  ");
                break;
            default:
                vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
                vga_puts("Unknown  ");
                break;
        }
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        /* Ticks */
        vga_put_dec(t->total_ticks);
        vga_puts("  ");

        /* Nombre */
        vga_puts(t->name);
        vga_putchar('\n');
    }
}

/* ── despachador ──────────────────────────────────────────── */

static void execute(char* line) {
    char copy[MAX_INPUT];
    strncpy(copy, line, MAX_INPUT - 1);
    copy[MAX_INPUT - 1] = '\0';

    char* argv[MAX_ARGS];
    int argc = split_args(copy, argv);
    if (argc == 0) return;

    /* Comandos existentes */
    if (strcmp(argv[0], "help")    == 0) { cmd_help(); return; }
    if (strcmp(argv[0], "clear")   == 0) { cmd_clear(); return; }
    if (strcmp(argv[0], "echo")    == 0) { cmd_echo(argc, argv); return; }
    if (strcmp(argv[0], "mem")     == 0) { cmd_mem(); return; }
    if (strcmp(argv[0], "uname")   == 0) { cmd_uname(); return; }
    if (strcmp(argv[0], "uptime")  == 0) { cmd_uptime(); return; }
    if (strcmp(argv[0], "history") == 0) { cmd_history(); return; }
    if (strcmp(argv[0], "colors")  == 0) { cmd_colors(); return; }
    if (strcmp(argv[0], "about")   == 0) { cmd_about(); return; }
    if (strcmp(argv[0], "reboot")  == 0) { cmd_reboot(); return; }
    if (strcmp(argv[0], "halt")    == 0) { cmd_halt(); return; }

    /* Comandos VFS */
    if (strcmp(argv[0], "ls")      == 0) { cmd_ls(argc, argv); return; }
    if (strcmp(argv[0], "pwd")     == 0) { cmd_pwd(); return; }
    if (strcmp(argv[0], "cd")      == 0) { cmd_cd(argc, argv); return; }
    if (strcmp(argv[0], "mkdir")   == 0) { cmd_mkdir(argc, argv); return; }
    if (strcmp(argv[0], "touch")   == 0) { cmd_touch(argc, argv); return; }
    if (strcmp(argv[0], "cat")     == 0) { cmd_cat(argc, argv); return; }
    if (strcmp(argv[0], "write")   == 0) { cmd_write(argc, argv); return; }
    if (strcmp(argv[0], "rm")      == 0) { cmd_rm(argc, argv); return; }

    /* Procesos */
    if (strcmp(argv[0], "ps")      == 0) { cmd_ps(); return; }

    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("Comando no encontrado: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(argv[0]);
    vga_puts("\nEscribe 'help' para ver los comandos disponibles.\n");
}

/* ── bucle principal ──────────────────────────────────────── */

void shell_run(void) {
    char input[MAX_INPUT];

    print_separator();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("Escribe 'help' para ver los comandos disponibles.\n");
    print_separator();

    while (1) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("fernando");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts("@");
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_puts("FernandoOS");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts(":~$ ");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        readline(input, MAX_INPUT);
        vga_putchar('\n');

        if (input[0] == '\0') continue;

        /* Guardar en historial */
        strncpy(history[hist_count % HISTORY_LEN], input, MAX_INPUT - 1);
        history[hist_count % HISTORY_LEN][MAX_INPUT - 1] = '\0';
        hist_count++;

        execute(input);
    }
}
