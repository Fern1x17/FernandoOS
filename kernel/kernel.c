#include "../cpu/gdt.h"
#include "../cpu/idt.h"
#include "../cpu/isr.h"
#include "../cpu/irq.h"
#include "../drivers/vga.h"
#include "../drivers/timer.h"
#include "../drivers/keyboard.h"
#include "../drivers/ata.h"
#include "../memory/mem.h"
#include "../memory/pmm.h"
#include "../memory/vmm.h"
#include "../memory/paging.h"
#include "../fs/vfs.h"
#include "../sched/sched.h"
#include "../users/users.h"
#include "../shell/shell.h"
#include <stdint.h>

#define MULTIBOOT_MAGIC 0x2BADB002

/* ── Tarea idle ───────────────────────────────────────────── */

static void task_idle(void) {
    volatile uint32_t counter = 0;
    while (1) {
        counter++;
        if (counter % 5000000 == 0)
            sched_yield();
    }
}

/* ── Utilidades de arranque ───────────────────────────────── */

static void banner(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  ___                               _      ___  ____  \n");
    vga_puts(" | __| ___  _ _  _ _   __ _ _ _  __| |___ / _ \\/ ___| \n");
    vga_puts(" | _| / -_)| '_|| ' \\ / _` | ' \\/ _` / _ \\ |  _\\__ \\ \n");
    vga_puts(" |_|  \\___||_|  |_||_|\\__,_|_||_\\__,_\\___/\\____/___/ \n");
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts("                               v0.3.0  -  kernel x86\n\n");
}

static void boot_msg(const char* component, int ok) {
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts("  [ ");
    if (ok) { vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK); vga_puts(" OK "); }
    else    { vga_set_color(VGA_COLOR_LIGHT_RED,   VGA_COLOR_BLACK); vga_puts("FAIL"); }
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_puts(" ] ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(component);
    vga_putchar('\n');
}

/* ── Punto de entrada del kernel ─────────────────────────── */

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
} __attribute__((packed)) mb_info_t;

void kernel_main(uint32_t magic, uint32_t mb_addr) {
    vga_init();
    vga_clear();
    banner();

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts(" Iniciando FernandoOS...\n\n");

    if (magic != MULTIBOOT_MAGIC) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts(" ADVERTENCIA: magic multiboot incorrecto: ");
        vga_put_hex(magic); vga_putchar('\n');
    }

    uint32_t total_mem_kb = 32U * 1024U;
    if (mb_addr) {
        mb_info_t* mbi = (mb_info_t*)mb_addr;
        if (mbi->flags & 1U) total_mem_kb = 1024U + mbi->mem_upper;
    }

    mem_init();
    boot_msg("Heap kmalloc (free-list + coalescing, 4 MB)", 1);

    pmm_init(total_mem_kb);
    boot_msg("PMM (bitmap de frames fisicos)", 1);

    paging_init();
    boot_msg("Paginacion (identity map 32 MB)", 1);

    vmm_init();
    boot_msg("VMM (gestor de memoria virtual)", 1);

    gdt_init();
    boot_msg("GDT (Global Descriptor Table)", 1);

    idt_init();
    boot_msg("IDT (Interrupt Descriptor Table)", 1);

    isr_install();
    boot_msg("ISR (Manejadores de excepciones)", 1);

    irq_install();
    boot_msg("IRQ (Controlador de interrupciones PIC)", 1);

    /* ATA debe inicializarse antes del VFS y usuarios para que
       vfs_load() y users_load() puedan leer del disco */
    ata_init();
    boot_msg(ata_present()
             ? "ATA PIO (disco IDE detectado)"
             : "ATA PIO (sin disco — sin persistencia)", 1);

    vfs_init();
    if (!vfs_load() && ata_present()) vfs_save();   /* primer arranque: persiste defaults */
    boot_msg("VFS (RAM filesystem + persistencia ATA)", 1);

    users_init();
    if (!users_load() && ata_present()) users_save(); /* primer arranque: persiste defaults */
    boot_msg("Usuarios (cuentas cargadas del disco)", 1);

    timer_install();
    boot_msg("PIT (Temporizador @ 100 Hz)", 1);

    keyboard_install();
    boot_msg("Teclado PS/2 ES-QWERTY (IRQ1)", 1);

    sched_init();
    boot_msg("Scheduler (round-robin)", 1);

    __asm__ volatile("sti");
    boot_msg("CPU (Interrupciones habilitadas)", 1);

    sched_create("idle", task_idle);
    sched_start();

    vga_putchar('\n');
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts(" Sistema listo.\n\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    shell_run();

    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}
