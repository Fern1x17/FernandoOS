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
#include "../users/users.h"
#include "../python/python.h"
#include <stdint.h>

#define MAX_INPUT   256
#define MAX_ARGS    16
#define HISTORY_LEN 8

static char history[HISTORY_LEN][MAX_INPUT];
static int  hist_count    = 0;
static int  logout_flag   = 0;

/* ── Lectura de línea ─────────────────────────────────────── */

static void readline(char* buf, int max) {
    int i = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n' || c == '\r') { buf[i] = '\0'; return; }
        if (c == '\b') {
            if (i > 0) { i--; vga_putchar('\b'); }
            continue;
        }
        if (c == 3) { buf[0] = '\0'; vga_puts("^C\n"); return; }
        if (i < max - 1) { buf[i++] = c; vga_putchar(c); }
    }
}

/* Lectura de contraseña: muestra '*' en lugar del carácter */
static void readline_secret(char* buf, int max) {
    int i = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n' || c == '\r') { buf[i] = '\0'; return; }
        if (c == '\b') {
            if (i > 0) { i--; vga_putchar('\b'); }
            continue;
        }
        if (c == 3) { buf[0] = '\0'; vga_puts("^C\n"); return; }
        if (i < max - 1) { buf[i++] = c; vga_putchar('*'); }
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

/* ── Helpers de presentación ──────────────────────────────── */

static void print_separator(void) {
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts("────────────────────────────────────────\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

/* ── Comandos del sistema ─────────────────────────────────── */

static void cmd_help(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Sistema:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  help              - Esta ayuda\n");
    vga_puts("  clear             - Limpia la pantalla\n");
    vga_puts("  echo <texto>      - Imprime texto\n");
    vga_puts("  uname             - Info del sistema\n");
    vga_puts("  uptime            - Tiempo desde arranque\n");
    vga_puts("  history           - Historial de comandos\n");
    vga_puts("  colors            - Paleta de 16 colores\n");
    vga_puts("  about             - Acerca de FernandoOS\n");
    vga_puts("  reboot            - Reiniciar\n");
    vga_puts("  halt              - Apagar\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Memoria:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  mem               - Estado completo de memoria\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Filesystem:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  ls [dir]          - Listar directorio\n");
    vga_puts("  pwd               - Ruta actual\n");
    vga_puts("  cd <dir>          - Cambiar directorio\n");
    vga_puts("  mkdir <nombre>    - Crear directorio\n");
    vga_puts("  touch <nombre>    - Crear fichero vacío\n");
    vga_puts("  cat <fichero>     - Mostrar contenido\n");
    vga_puts("  write <f> <txt>   - Escribir en fichero\n");
    vga_puts("  rm <nombre>       - Eliminar fichero/dir\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Programacion:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  python <fichero>  - Ejecutar script Python\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Procesos:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  ps                - Tabla de procesos\n");

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Usuarios:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  whoami            - Usuario actual\n");
    vga_puts("  users             - Lista de usuarios\n");
    vga_puts("  passwd [usuario]  - Cambiar contraseña\n");
    vga_puts("  adduser <n> <uid> - Agregar usuario (root)\n");
    vga_puts("  deluser <n>       - Eliminar usuario (root)\n");
    vga_puts("  su <usuario>      - Cambiar de usuario\n");
    vga_puts("  logout            - Cerrar sesión\n");
}

static void cmd_clear(void) { vga_clear(); }

static void cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) vga_putchar(' ');
        vga_puts(argv[i]);
    }
    vga_putchar('\n');
}

static void cmd_mem(void) {
    /* ── PMM ─────────────────────────────────────────────── */
    uint32_t ptotal = pmm_get_total();
    uint32_t pused  = pmm_get_used();
    uint32_t pfree  = pmm_get_free();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Memoria física (PMM — bitmap de frames):\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Total    : "); vga_put_dec(ptotal * PAGE_SIZE / 1024);
    vga_puts(" KiB  ("); vga_put_dec(ptotal); vga_puts(" frames)\n");
    vga_puts("  Kernel   : "); vga_put_dec(PMM_KERN_FRAMES * PAGE_SIZE / 1024);
    vga_puts(" KiB  reservados (0x00000000–0x007FFFFF)\n");
    vga_puts("  Usado    : "); vga_put_dec(pused * PAGE_SIZE / 1024);
    vga_puts(" KiB  ("); vga_put_dec(pused); vga_puts(" frames)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("  Libre    : "); vga_put_dec(pfree * PAGE_SIZE / 1024);
    vga_puts(" KiB  ("); vga_put_dec(pfree); vga_puts(" frames)\n");

    /* ── Heap kmalloc (free-list) ────────────────────────── */
    uint32_t hused    = mem_get_used();
    uint32_t hfree    = mem_get_free();
    uint32_t htotal   = mem_get_total();
    uint32_t hblocks  = mem_get_blocks();
    uint32_t hlargest = mem_get_largest_free();

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("Heap kmalloc (free-list + coalescing):\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Rango    : 0x00400000 – 0x007FFFFF\n");
    vga_puts("  Total    : "); vga_put_dec(htotal / 1024); vga_puts(" KiB\n");
    vga_puts("  Usado    : "); vga_put_dec(hused  / 1024); vga_puts(" KiB\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("  Libre    : "); vga_put_dec(hfree  / 1024); vga_puts(" KiB\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  Bloques  : "); vga_put_dec(hblocks); vga_puts("\n");
    vga_puts("  Max libre: "); vga_put_dec(hlargest / 1024); vga_puts(" KiB\n");
}

static void cmd_uname(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("FernandoOS");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(" v0.2.0  i686  #1  (kernel freestanding)\n");
    vga_puts("Arquitectura: x86 32-bit  |  Modo: protegido\n");
    vga_puts("Boot:         GRUB Multiboot  |  VM: QEMU\n");
}

static void cmd_uptime(void) {
    uint32_t ticks = timer_get_ticks();
    uint32_t secs  = ticks / 100;
    uint32_t mins  = secs / 60;
    uint32_t hrs   = mins / 60;
    mins %= 60; secs %= 60;
    vga_puts("Activo: ");
    if (hrs) { vga_put_dec(hrs); vga_puts("h "); }
    vga_put_dec(mins); vga_puts("m ");
    vga_put_dec(secs); vga_puts("s  (ticks="); vga_put_dec(ticks); vga_puts(")\n");
}

static void cmd_history(void) {
    if (!hist_count) { vga_puts("Sin historial.\n"); return; }
    int start = hist_count > HISTORY_LEN ? hist_count - HISTORY_LEN : 0;
    for (int i = start; i < hist_count; i++) {
        vga_puts("  "); vga_put_dec((uint32_t)(i + 1)); vga_puts("  ");
        vga_puts(history[i % HISTORY_LEN]); vga_putchar('\n');
    }
}

static void cmd_colors(void) {
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("Paleta VGA (16 colores):\n");
    for (int i = 0; i < 16; i++) {
        vga_set_color((vga_color_t)i, VGA_COLOR_BLACK);
        vga_puts("  Color "); vga_put_dec((uint32_t)i); vga_puts("  ABCDEF  ");
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
    vga_puts(" | _| / -_)| '_|| ' \\/ _` | ' \\/ _` / _ \\ |  _\\__ \\\n");
    vga_puts(" |_|  \\___||_|  |_||_|\\__,_|_||_\\__,_\\___/\\____/___/\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("\n  Un sistema operativo Unix-like hecho desde cero.\n");
    vga_puts("  Arquitectura: x86 32-bit (modo protegido)\n");
    vga_puts("  Creado por:   Fernando\n");
    vga_puts("  Version:      0.2.0\n");
    vga_puts("  Licencia:     MIT\n");
}

static void cmd_reboot(void) {
    vga_puts("Reiniciando...\n");
    timer_sleep(500);
    uint8_t tmp;
    do { tmp = inb(0x64); } while (tmp & 0x02);
    outb(0x64, 0xFE);
    __asm__ volatile("cli; hlt");
}

static void cmd_halt(void) {
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts("Sistema detenido. Puede apagar la máquina virtual.\n");
    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

/* ── Comandos VFS ─────────────────────────────────────────── */

static void cmd_ls(int argc, char** argv) {
    vfs_ls(argc < 2 ? "." : argv[1]);
}

static void cmd_pwd(void) { vfs_pwd(); }

static void cmd_cd(int argc, char** argv) {
    vfs_cd(argc < 2 ? "/" : argv[1]);
}

static void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) { vga_puts("mkdir: falta el nombre\n"); return; }
    if (vfs_mkdir(argv[1]) < 0) vga_puts("mkdir: error al crear el directorio\n");
}

static void cmd_touch(int argc, char** argv) {
    if (argc < 2) { vga_puts("touch: falta el nombre\n"); return; }
    vfs_create(argv[1]);
}

static void cmd_cat(int argc, char** argv) {
    if (argc < 2) { vga_puts("cat: falta el nombre del fichero\n"); return; }
    vfs_cat(argv[1]);
}

static void cmd_write(int argc, char** argv) {
    if (argc < 3) { vga_puts("write: uso: write <fichero> <texto...>\n"); return; }
    char buf[VFS_MAX_DATA];
    int pos = 0;
    for (int i = 2; i < argc && pos < (int)(VFS_MAX_DATA - 1); i++) {
        if (i > 2 && pos < (int)(VFS_MAX_DATA - 2)) buf[pos++] = ' ';
        const char* s = argv[i];
        while (*s && pos < (int)(VFS_MAX_DATA - 1)) {
            if (s[0] == '\\' && s[1] == 'n') { buf[pos++] = '\n'; s += 2; }
            else if (s[0] == '\\' && s[1] == 't') { buf[pos++] = '\t'; s += 2; }
            else buf[pos++] = *s++;
        }
    }
    buf[pos] = '\0';
    if (vfs_write(argv[1], buf, (uint32_t)pos) < 0)
        vga_puts("write: error al escribir\n");
}

static void cmd_python(int argc, char** argv) {
    if (argc < 2) { vga_puts("python: uso: python <fichero.py>\n"); return; }
    char source[VFS_MAX_DATA];
    int len = vfs_read(argv[1], source, VFS_MAX_DATA);
    if (len < 0) {
        vga_puts("python: fichero no encontrado: ");
        vga_puts(argv[1]); vga_putchar('\n');
        return;
    }
    python_run(source);
}

static void cmd_rm(int argc, char** argv) {
    if (argc < 2) { vga_puts("rm: falta el nombre\n"); return; }
    vfs_rm(argv[1]);
}

/* ── Comando ps ───────────────────────────────────────────── */

static void cmd_ps(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("PID  ESTADO    TICKS      NOMBRE\n");
    vga_puts("---  --------  ---------  --------\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t* t = sched_get_task(i);
        if (!t || t->state == TASK_DEAD) continue;
        vga_put_dec(t->pid); vga_puts("    ");
        switch (t->state) {
            case TASK_RUNNING:
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                vga_puts("Running  "); break;
            case TASK_READY:
                vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                vga_puts("Ready    "); break;
            case TASK_BLOCKED:
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                vga_puts("Blocked  "); break;
            default:
                vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
                vga_puts("Unknown  "); break;
        }
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_put_dec(t->total_ticks); vga_puts("  ");
        vga_puts(t->name); vga_putchar('\n');
    }
}

/* ── Comandos de usuario ──────────────────────────────────── */

static void cmd_whoami(void) {
    user_t* u = users_current();
    if (!u) { vga_puts("(sin sesión)\n"); return; }
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts(u->name);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("  (uid="); vga_put_dec(u->uid); vga_puts(")");
    vga_puts("  home="); vga_puts(u->home);
    vga_putchar('\n');
}

static void cmd_users(void) {
    users_list_all();
}

static void cmd_passwd(int argc, char** argv) {
    user_t* cur = users_current();
    const char* target_name = (argc >= 2) ? argv[1] : (cur ? cur->name : 0);

    if (!target_name) { vga_puts("passwd: no hay usuario activo\n"); return; }

    /* No-root sólo puede cambiar su propia contraseña */
    if (cur && cur->uid != USERS_ROOT_UID && strcmp(target_name, cur->name) != 0) {
        vga_puts("passwd: permiso denegado\n"); return;
    }

    char old_pass[64], new_pass[64], confirm[64];

    /* Si no es root, pedir contraseña actual */
    if (!cur || cur->uid != USERS_ROOT_UID) {
        vga_puts("Contraseña actual: ");
        readline_secret(old_pass, sizeof(old_pass));
        vga_putchar('\n');
    } else {
        old_pass[0] = '\0';
    }

    vga_puts("Nueva contraseña:   ");
    readline_secret(new_pass, sizeof(new_pass));
    vga_putchar('\n');

    vga_puts("Confirmar contraseña: ");
    readline_secret(confirm, sizeof(confirm));
    vga_putchar('\n');

    if (strcmp(new_pass, confirm) != 0) {
        vga_puts("passwd: las contraseñas no coinciden\n"); return;
    }
    if (!new_pass[0]) {
        vga_puts("passwd: la contraseña no puede estar vacía\n"); return;
    }

    int r = users_passwd(target_name,
                         (cur && cur->uid == USERS_ROOT_UID) ? 0 : old_pass,
                         new_pass);
    if (r == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Contraseña actualizada.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else {
        vga_puts("passwd: error (contraseña actual incorrecta)\n");
    }
}

static void cmd_adduser(int argc, char** argv) {
    user_t* cur = users_current();
    if (!cur || cur->uid != USERS_ROOT_UID) {
        vga_puts("adduser: se requiere ser root\n"); return;
    }
    if (argc < 3) {
        vga_puts("adduser: uso: adduser <nombre> <uid>\n"); return;
    }

    /* Parsear uid manualmente */
    uint32_t uid = 0;
    for (const char* p = argv[2]; *p; p++) {
        if (*p < '0' || *p > '9') { vga_puts("adduser: uid inválido\n"); return; }
        uid = uid * 10 + (uint32_t)(*p - '0');
    }

    char pass[64], confirm[64];
    vga_puts("Contraseña para ");
    vga_puts(argv[1]);
    vga_puts(": ");
    readline_secret(pass, sizeof(pass));
    vga_putchar('\n');

    if (!pass[0]) { vga_puts("adduser: contraseña vacía no permitida\n"); return; }

    vga_puts("Confirmar contraseña: ");
    readline_secret(confirm, sizeof(confirm));
    vga_putchar('\n');

    if (strcmp(pass, confirm) != 0) {
        vga_puts("adduser: las contraseñas no coinciden\n"); return;
    }

    int r = users_add(argv[1], pass, uid);
    if (r >= 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Usuario '"); vga_puts(argv[1]); vga_puts("' creado.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else {
        vga_puts("adduser: error (nombre duplicado o tabla llena)\n");
    }
}

static void cmd_deluser(int argc, char** argv) {
    user_t* cur = users_current();
    if (!cur || cur->uid != USERS_ROOT_UID) {
        vga_puts("deluser: se requiere ser root\n"); return;
    }
    if (argc < 2) { vga_puts("deluser: uso: deluser <nombre>\n"); return; }

    int r = users_del(argv[1]);
    if (r == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Usuario '"); vga_puts(argv[1]); vga_puts("' eliminado.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else {
        vga_puts("deluser: error (no existe, es root, o sesión activa)\n");
    }
}

static void cmd_su(int argc, char** argv) {
    if (argc < 2) { vga_puts("su: uso: su <usuario>\n"); return; }

    user_t* target = users_get_by_name(argv[1]);
    if (!target) { vga_puts("su: usuario no encontrado\n"); return; }

    user_t* cur = users_current();

    /* root puede cambiar sin contraseña */
    if (cur && cur->uid == USERS_ROOT_UID) {
        users_switch(argv[1]);
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Sesión cambiada a '"); vga_puts(argv[1]); vga_puts("'.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        return;
    }

    char pass[64];
    vga_puts("Contraseña de ");
    vga_puts(argv[1]);
    vga_puts(": ");
    readline_secret(pass, sizeof(pass));
    vga_putchar('\n');

    if (users_login(argv[1], pass) >= 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("Sesión cambiada a '"); vga_puts(argv[1]); vga_puts("'.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else {
        vga_puts("su: autenticación fallida\n");
    }
}

static void cmd_logout(void) {
    user_t* u = users_current();
    if (u) {
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        vga_puts("Cerrando sesión de '");
        vga_puts(u->name);
        vga_puts("'...\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
    logout_flag = 1;
}

/* ── Despachador ──────────────────────────────────────────── */

static void execute(char* line) {
    char copy[MAX_INPUT];
    strncpy(copy, line, MAX_INPUT - 1);
    copy[MAX_INPUT - 1] = '\0';

    char* argv[MAX_ARGS];
    int argc = split_args(copy, argv);
    if (!argc) return;

    /* Sistema */
    if (strcmp(argv[0], "help")    == 0) { cmd_help();              return; }
    if (strcmp(argv[0], "clear")   == 0) { cmd_clear();             return; }
    if (strcmp(argv[0], "echo")    == 0) { cmd_echo(argc, argv);    return; }
    if (strcmp(argv[0], "mem")     == 0) { cmd_mem();               return; }
    if (strcmp(argv[0], "uname")   == 0) { cmd_uname();             return; }
    if (strcmp(argv[0], "uptime")  == 0) { cmd_uptime();            return; }
    if (strcmp(argv[0], "history") == 0) { cmd_history();           return; }
    if (strcmp(argv[0], "colors")  == 0) { cmd_colors();            return; }
    if (strcmp(argv[0], "about")   == 0) { cmd_about();             return; }
    if (strcmp(argv[0], "reboot")  == 0) { cmd_reboot();            return; }
    if (strcmp(argv[0], "halt")    == 0) { cmd_halt();              return; }

    /* Filesystem */
    if (strcmp(argv[0], "ls")      == 0) { cmd_ls(argc, argv);     return; }
    if (strcmp(argv[0], "pwd")     == 0) { cmd_pwd();              return; }
    if (strcmp(argv[0], "cd")      == 0) { cmd_cd(argc, argv);     return; }
    if (strcmp(argv[0], "mkdir")   == 0) { cmd_mkdir(argc, argv);  return; }
    if (strcmp(argv[0], "touch")   == 0) { cmd_touch(argc, argv);  return; }
    if (strcmp(argv[0], "cat")     == 0) { cmd_cat(argc, argv);    return; }
    if (strcmp(argv[0], "write")   == 0) { cmd_write(argc, argv);  return; }
    if (strcmp(argv[0], "rm")      == 0) { cmd_rm(argc, argv);     return; }

    /* Python */
    if (strcmp(argv[0], "python")  == 0) { cmd_python(argc, argv); return; }

    /* Procesos */
    if (strcmp(argv[0], "ps")      == 0) { cmd_ps();               return; }

    /* Usuarios */
    if (strcmp(argv[0], "whoami")  == 0) { cmd_whoami();              return; }
    if (strcmp(argv[0], "users")   == 0) { cmd_users();               return; }
    if (strcmp(argv[0], "passwd")  == 0) { cmd_passwd(argc, argv);    return; }
    if (strcmp(argv[0], "adduser") == 0) { cmd_adduser(argc, argv);   return; }
    if (strcmp(argv[0], "deluser") == 0) { cmd_deluser(argc, argv);   return; }
    if (strcmp(argv[0], "su")      == 0) { cmd_su(argc, argv);        return; }
    if (strcmp(argv[0], "logout")  == 0) { cmd_logout();              return; }

    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("Comando no encontrado: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(argv[0]);
    vga_puts("\nEscribe 'help' para ver los comandos disponibles.\n");
}

/* ── Login ────────────────────────────────────────────────── */

static void shell_do_login(void) {
    char name[USERS_NAME_LEN];
    char pass[64];
    int  attempts = 0;

    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts("\n────────────────────────────────────────\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    while (1) {
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_puts("FernandoOS login: ");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        readline(name, USERS_NAME_LEN);
        vga_putchar('\n');

        if (!name[0]) continue;

        vga_puts("Contraseña:        ");
        readline_secret(pass, sizeof(pass));
        vga_putchar('\n');

        if (users_login(name, pass) >= 0) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts("\nBienvenido, ");
            vga_puts(name);
            vga_puts("!\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            return;
        }

        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("Login incorrecto.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

        attempts++;
        if (attempts >= 3) {
            vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            vga_puts("Demasiados intentos. Intenta de nuevo.\n\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            attempts = 0;
        }
    }
}

/* ── Bucle principal ──────────────────────────────────────── */

void shell_run(void) {
    char input[MAX_INPUT];

    while (1) {
        logout_flag = 0;
        shell_do_login();

        print_separator();
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_puts("Escribe 'help' para ver los comandos disponibles.\n");
        print_separator();

        while (!logout_flag) {
            user_t* u = users_current();
            const char* uname = u ? u->name : "?";

            /* Prompt: usuario@FernandoOS:~$ */
            if (u && u->uid == USERS_ROOT_UID)
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            else
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts(uname);
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_puts("@");
            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            vga_puts("FernandoOS");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            /* root muestra # en lugar de $ */
            vga_puts(u && u->uid == USERS_ROOT_UID ? ":~# " : ":~$ ");

            readline(input, MAX_INPUT);
            vga_putchar('\n');

            if (!input[0]) continue;

            strncpy(history[hist_count % HISTORY_LEN], input, MAX_INPUT - 1);
            history[hist_count % HISTORY_LEN][MAX_INPUT - 1] = '\0';
            hist_count++;

            execute(input);
        }

        users_logout();
    }
}
