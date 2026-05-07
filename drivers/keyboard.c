#include "keyboard.h"
#include "../cpu/irq.h"
#include "../cpu/ports.h"
#include <stdint.h>

/*
 * Teclado espanol ES-QWERTY, scancodes set 1.
 * Se usan uint8_t en lugar de char para poder almacenar
 * codigos CP437 > 127 sin signo.
 */

/* Normal (sin modificadores) */
static const uint8_t sc_lower[] = {
    0,    27,   '1', '2', '3', '4', '5', '6', '7', '8',  /* 0x00-0x09 */
    '9',  '0', '\'', 0xAD,'\b','\t', 'q', 'w', 'e', 'r', /* 0x0A-0x13: ¡=0xAD */
    't',  'y',  'u', 'i', 'o', 'p', '`', '+','\n', 0,    /* 0x14-0x1D */
    'a',  's',  'd', 'f', 'g', 'h', 'j', 'k', 'l',0xA4,  /* 0x1E-0x27: n~=0xA4 CP437 */
    0xB4, 0xBA,  0, 0xE7, 'z', 'x', 'c', 'v', 'b', 'n',  /* 0x28-0x31: ´=0xB4,°=0xBA,ç=0xE7 */
    'm',  ',',  '.', '-',  0,  '*',  0,  ' ',  0,   0,   /* 0x32-0x3B */
    0,    0,    0,   0,   0,   0,   0,  '7', '8', '9',   /* 0x3C-0x45 */
    '-',  '4',  '5', '6', '+', '1', '2', '3', '0', '.'   /* 0x46-0x4F */
};

/* Con Shift */
static const uint8_t sc_upper[] = {
    0,    27,   '!', '"', 0xB7, '$', '%', '&', '/', '(',  /* 0x00-0x09: ·=0xB7 CP437 */
    ')',  '=',  '?', 0xAD,'\b','\t','Q', 'W', 'E', 'R',  /* 0x0A-0x13: ¡=0xAD */
    'T',  'Y',  'U', 'I', 'O', 'P', '^', '*','\n', 0,    /* 0x14-0x1D */
    'A',  'S',  'D', 'F', 'G', 'H', 'J', 'K', 'L',0xA5,  /* 0x1E-0x27: N~=0xA5 CP437 */
    0xA8, 0xF8,  0, 'C',  'Z', 'X', 'C', 'V', 'B', 'N',  /* 0x28-0x31: ¿=0xA8 */
    'M',  ';',  ':', '_',  0,  '*',  0,  ' ',  0,   0,   /* 0x32-0x3B */
    0,    0,    0,   0,   0,   0,   0,  '7', '8', '9',   /* 0x3C-0x45 */
    '-',  '4',  '5', '6', '+', '1', '2', '3', '0', '.'   /* 0x46-0x4F */
};

/*
 * Tabla AltGr — caracteres especiales accesibles con AltGr.
 * El indice es el scancode; 0 = no hay caracter AltGr para ese SC.
 * Vocales acentuadas: AltGr+a=á, e=é, i=í, o=ó, u=ú, q=æ
 */
static const uint8_t sc_altgr[] = {
    0,    0,    0,   '@', '#', '~',  0,   0,   0,   0,   /* 0x00-0x09: 2->@, 3->#, 4->~ */
    0,    0,    0,   0,   0,   0,  0x91,  0, 0x82,  0,   /* 0x0A-0x13: q->æ(0x91), e->é(0x82) */
    0,    0,  0xA3,0xA1,0xA2,  0,  '[',  ']',  0,   0,   /* 0x14-0x1D: u->ú, i->í, o->ó, `->[ +->&gt;] */
  0xA0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   /* 0x1E-0x27: a->á(0xA0) */
    '{',  0,    0,  '}',  0,   0,   0,   0,   0,   0,   /* 0x28-0x31: ´->{, ç->} */
    0,    0,    0,  '\\', 0,   0,   0,   0,   0,   0,   /* 0x32-0x3B: ->\\ */
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0,   /* 0x3C-0x45 */
    0,    0,    0,   0,   0,   0,   0,   0,   0,   0    /* 0x46-0x4F */
};

#define SC_LOWER_SIZE ((int)(sizeof(sc_lower)/sizeof(sc_lower[0])))
#define SC_ALTGR_SIZE ((int)(sizeof(sc_altgr)/sizeof(sc_altgr[0])))

#define KB_BUF_SIZE 256

static char     kb_buf[KB_BUF_SIZE];
static volatile int kb_head = 0;
static volatile int kb_tail = 0;

static int shift       = 0;
static int ctrl        = 0;
static int caps        = 0;
static int altgr       = 0;  /* Alt izquierdo o AltGr */
static int dead_accent = 0;  /* tecla muerta ´ pendiente */

static void kb_buf_put(char c) {
    int next = (kb_tail + 1) % KB_BUF_SIZE;
    if (next != kb_head) {
        kb_buf[kb_tail] = c;
        kb_tail = next;
    }
}

static void keyboard_handler(registers_t* regs) {
    (void)regs;
    uint8_t sc = inb(0x60);

    if (sc & 0x80) {
        /* Tecla liberada */
        uint8_t rel = sc & 0x7F;
        if (rel == 0x2A || rel == 0x36) shift = 0;
        if (rel == 0x1D)                ctrl  = 0;
        if (rel == 0x38)                altgr = 0;
        return;
    }

    /* Tecla presionada */
    if (sc == 0x2A || sc == 0x36) { shift = 1; return; }
    if (sc == 0x1D)                { ctrl  = 1; return; }
    if (sc == 0x38)                { altgr = 1; return; }
    if (sc == 0x3A)                { caps  = !caps; return; }

    if (sc >= SC_LOWER_SIZE) return;

    uint8_t c = 0;

    /* AltGr tiene maxima prioridad */
    if (altgr && sc < SC_ALTGR_SIZE && sc_altgr[sc] != 0) {
        c = sc_altgr[sc];
    } else if (shift) {
        c = sc_upper[sc];
    } else {
        c = sc_lower[sc];
    }

    /* Caps lock solo afecta letras ASCII */
    if (!altgr && caps) {
        if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 32);
        else if (c >= 'A' && c <= 'Z') c = (uint8_t)(c + 32);
    }

    /* Ctrl+C = caracter especial 3 */
    if (ctrl && (c == 'c' || c == 'C')) c = 3;

    /* Tecla muerta: acento agudo (´).
     * Si dead_accent esta activo, la siguiente vocal produce la versión acentuada.
     * Cualquier otra tecla emite ´ seguido del caracter normal. */
    if (dead_accent && c) {
        dead_accent = 0;
        uint8_t out;
        switch (c) {
            case 'a': out = 0xA0; break;  /* á */
            case 'e': out = 0x82; break;  /* é */
            case 'i': out = 0xA1; break;  /* í */
            case 'o': out = 0xA2; break;  /* ó */
            case 'u': out = 0xA3; break;  /* ú */
            case 'A': out = 0xB5; break;  /* Á */
            case 'E': out = 0x90; break;  /* É */
            case 'I': out = 0xD6; break;  /* Í */
            case 'O': out = 0xE0; break;  /* Ó */
            case 'U': out = 0xE9; break;  /* Ú */
            default:  kb_buf_put((char)0xB4); out = c; break;
        }
        if (out) kb_buf_put((char)out);
        return;
    }

    /* Activar tecla muerta ´ (solo sin AltGr para no bloquear ´->{) */
    if (!altgr && c == 0xB4) {
        dead_accent = 1;
        return;
    }

    if (c) kb_buf_put((char)c);
}

void keyboard_install(void) {
    irq_install_handler(1, keyboard_handler);
}

char keyboard_getchar(void) {
    while (kb_head == kb_tail)
        __asm__ volatile("hlt");
    char c = kb_buf[kb_head];
    kb_head = (kb_head + 1) % KB_BUF_SIZE;
    return c;
}

int keyboard_has_char(void) {
    return kb_head != kb_tail;
}
