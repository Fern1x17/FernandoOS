#include "ata.h"
#include "../cpu/ports.h"

/* ── Registros ATA — canal primario ──────────────────────── */
#define ATA_DATA   0x1F0
#define ATA_FEAT   0x1F1
#define ATA_COUNT  0x1F2
#define ATA_LBA0   0x1F3
#define ATA_LBA1   0x1F4
#define ATA_LBA2   0x1F5
#define ATA_DRIVE  0x1F6
#define ATA_CMD    0x1F7
#define ATA_STATUS 0x1F7
#define ATA_CTRL   0x3F6   /* alternate status / device control */

/* ── Bits de estado ───────────────────────────────────────── */
#define SR_BSY  0x80
#define SR_DRQ  0x08
#define SR_ERR  0x01

/* ── Comandos ─────────────────────────────────────────────── */
#define CMD_READ   0x20
#define CMD_WRITE  0x30
#define CMD_FLUSH  0xE7

static int disk_ok = 0;

/* 400 ns de espera leyendo el registro de control 4 veces */
static void delay400(void) {
    inb(ATA_CTRL); inb(ATA_CTRL); inb(ATA_CTRL); inb(ATA_CTRL);
}

static void wait_bsy(void) {
    while (inb(ATA_STATUS) & SR_BSY);
}

static int wait_drq(void) {
    for (uint32_t i = 0; i < 300000U; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & SR_ERR) return 0;
        if (s & SR_DRQ) return 1;
    }
    return 0;   /* timeout */
}

/* ── Inicialización ───────────────────────────────────────── */

void ata_init(void) {
    disk_ok = 0;

    /* Soft reset del canal */
    outb(ATA_CTRL, 0x04);
    delay400();
    outb(ATA_CTRL, 0x00);
    delay400();

    /* Seleccionar master (LBA28) */
    outb(ATA_DRIVE, 0xA0);
    delay400();

    /* Si status == 0xFF no hay disco */
    if (inb(ATA_STATUS) == 0xFF) return;

    /* Esperar a que BSY se limpie */
    for (uint32_t t = 0; t < 1000000U; t++) {
        if (!(inb(ATA_STATUS) & SR_BSY)) { disk_ok = 1; return; }
    }
    /* Timeout: sin disco */
}

int ata_present(void) { return disk_ok; }

/* ── Helpers LBA ──────────────────────────────────────────── */

static void lba_setup(uint32_t lba) {
    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));  /* master, LBA28 */
    outb(ATA_FEAT,  0x00);
    outb(ATA_COUNT, 1);
    outb(ATA_LBA0,  (uint8_t)(lba));
    outb(ATA_LBA1,  (uint8_t)(lba >> 8));
    outb(ATA_LBA2,  (uint8_t)(lba >> 16));
}

/* ── Lectura ──────────────────────────────────────────────── */

void ata_read_sector(uint32_t lba, void* buf) {
    if (!disk_ok) return;
    wait_bsy();
    lba_setup(lba);
    outb(ATA_CMD, CMD_READ);
    delay400();
    if (!wait_drq()) return;

    uint16_t* p = (uint16_t*)buf;
    for (int i = 0; i < 256; i++) p[i] = inw(ATA_DATA);
}

/* ── Escritura ────────────────────────────────────────────── */

void ata_write_sector(uint32_t lba, const void* buf) {
    if (!disk_ok) return;
    wait_bsy();
    lba_setup(lba);
    outb(ATA_CMD, CMD_WRITE);
    delay400();
    if (!wait_drq()) return;

    const uint16_t* p = (const uint16_t*)buf;
    for (int i = 0; i < 256; i++) outw(ATA_DATA, p[i]);

    /* Flush cache para garantizar escritura */
    outb(ATA_CMD, CMD_FLUSH);
    wait_bsy();
}
