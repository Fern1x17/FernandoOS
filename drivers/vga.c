#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((uint16_t*)0xB8000)

static uint16_t* vga_buf;
static uint8_t   vga_color;
static int       vga_row;
static int       vga_col;

static inline uint8_t make_color(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)fg | ((uint8_t)bg << 4);
}

static inline uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)(uint8_t)c | ((uint16_t)color << 8);
}

void vga_init(void) {
    vga_buf   = VGA_MEMORY;
    vga_color = make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_row   = 0;
    vga_col   = 0;
}

void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_color = make_color(fg, bg);
}

void vga_clear(void) {
    uint16_t blank = make_entry(' ', vga_color);
    for (int y = 0; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = blank;
    vga_row = 0;
    vga_col = 0;
}

static void vga_scroll(void) {
    for (int y = 0; y < VGA_HEIGHT - 1; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = vga_buf[(y + 1) * VGA_WIDTH + x];

    uint16_t blank = make_entry(' ', vga_color);
    for (int x = 0; x < VGA_WIDTH; x++)
        vga_buf[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;

    vga_row = VGA_HEIGHT - 1;
}

void vga_set_pos(int row, int col) {
    vga_row = row;
    vga_col = col;
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        if (++vga_row == VGA_HEIGHT) vga_scroll();
        return;
    }
    if (c == '\r') { vga_col = 0; return; }
    if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buf[vga_row * VGA_WIDTH + vga_col] = make_entry(' ', vga_color);
        }
        return;
    }
    if (c == '\t') {
        int spaces = 4 - (vga_col % 4);
        for (int i = 0; i < spaces; i++) vga_putchar(' ');
        return;
    }
    vga_buf[vga_row * VGA_WIDTH + vga_col] = make_entry(c, vga_color);
    if (++vga_col == VGA_WIDTH) {
        vga_col = 0;
        if (++vga_row == VGA_HEIGHT) vga_scroll();
    }
}

void vga_puts(const char* str) {
    while (*str) vga_putchar(*str++);
}

void vga_put_hex(uint32_t val) {
    char buf[11];
    buf[0]  = '0';
    buf[1]  = 'x';
    buf[10] = '\0';
    for (int i = 9; i >= 2; i--) {
        int nibble = (int)(val & 0xF);
        buf[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        val >>= 4;
    }
    vga_puts(buf);
}

void vga_put_dec(uint32_t val) {
    if (val == 0) { vga_putchar('0'); return; }
    char buf[12];
    int i = 10;
    buf[11] = '\0';
    while (val > 0) {
        buf[i--] = (char)('0' + (val % 10));
        val /= 10;
    }
    vga_puts(&buf[i + 1]);
}

int vga_get_col(void) { return vga_col; }
int vga_get_row(void) { return vga_row; }
