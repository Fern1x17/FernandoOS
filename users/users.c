#include "users.h"
#include "../libc/string.h"
#include "../drivers/vga.h"
#include "../drivers/ata.h"

static user_t  users[USERS_MAX];
static user_t* current_user = 0;

/* ── Hash djb2 para contraseñas ───────────────────────────── */

static uint32_t djb2(const char* s) {
    uint32_t h = 5381U;
    while (*s) h = ((h << 5) + h) ^ (uint32_t)(unsigned char)*s++;
    return h;
}

/* ── Inicialización ───────────────────────────────────────── */

void users_init(void) {
    memset(users, 0, sizeof(users));

    /* root: uid=0, password="root" */
    users[0].used     = 1;
    users[0].uid      = 0;
    users[0].pwd_hash = djb2("root");
    strncpy(users[0].name, "root",     USERS_NAME_LEN - 1);
    strncpy(users[0].home, "/root",    USERS_HOME_LEN - 1);

    /* fernando: uid=1000, password="fernando" */
    users[1].used     = 1;
    users[1].uid      = 1000;
    users[1].pwd_hash = djb2("fernando");
    strncpy(users[1].name, "fernando",       USERS_NAME_LEN - 1);
    strncpy(users[1].home, "/home/fernando", USERS_HOME_LEN - 1);

    current_user = 0;
}

/* ── Autenticación ────────────────────────────────────────── */

int users_login(const char* name, const char* password) {
    uint32_t h = djb2(password);
    for (int i = 0; i < USERS_MAX; i++) {
        if (!users[i].used) continue;
        if (strcmp(users[i].name, name) == 0 && users[i].pwd_hash == h) {
            current_user = &users[i];
            return (int)users[i].uid;
        }
    }
    return -1;
}

void users_logout(void) {
    current_user = 0;
}

void users_switch(const char* name) {
    for (int i = 0; i < USERS_MAX; i++) {
        if (users[i].used && strcmp(users[i].name, name) == 0) {
            current_user = &users[i];
            return;
        }
    }
}

/* ── Consulta ─────────────────────────────────────────────── */

user_t* users_current(void)               { return current_user; }

user_t* users_get_by_name(const char* name) {
    for (int i = 0; i < USERS_MAX; i++)
        if (users[i].used && strcmp(users[i].name, name) == 0)
            return &users[i];
    return 0;
}

user_t* users_get_by_uid(uint32_t uid) {
    for (int i = 0; i < USERS_MAX; i++)
        if (users[i].used && users[i].uid == uid)
            return &users[i];
    return 0;
}

int users_count(void) {
    int n = 0;
    for (int i = 0; i < USERS_MAX; i++) if (users[i].used) n++;
    return n;
}

/* ── Gestión ──────────────────────────────────────────────── */

int users_add(const char* name, const char* password, uint32_t uid) {
    if (!name || !*name || !password) return -1;
    if (users_get_by_name(name)) return -1;   /* ya existe */

    for (int i = 0; i < USERS_MAX; i++) {
        if (users[i].used) continue;

        users[i].used     = 1;
        users[i].uid      = uid;
        users[i].pwd_hash = djb2(password);
        strncpy(users[i].name, name, USERS_NAME_LEN - 1);
        users[i].name[USERS_NAME_LEN - 1] = '\0';

        /* /home/<name> */
        strncpy(users[i].home, "/home/", USERS_HOME_LEN - 1);
        users[i].home[USERS_HOME_LEN - 1] = '\0';
        int pos = (int)strlen(users[i].home);
        strncpy(users[i].home + pos, name, USERS_HOME_LEN - 1 - pos);
        users[i].home[USERS_HOME_LEN - 1] = '\0';

        users_save();
        return (int)uid;
    }
    return -1;   /* tabla llena */
}

int users_del(const char* name) {
    if (!name) return -1;
    for (int i = 0; i < USERS_MAX; i++) {
        if (!users[i].used || strcmp(users[i].name, name) != 0) continue;
        if (users[i].uid == USERS_ROOT_UID)     return -1;   /* no borrar root */
        if (current_user == &users[i])           return -1;   /* sesión activa */
        memset(&users[i], 0, sizeof(user_t));
        users_save();
        return 0;
    }
    return -1;
}

int users_passwd(const char* name, const char* old_pwd, const char* new_pwd) {
    user_t* u = users_get_by_name(name);
    if (!u || !new_pwd) return -1;

    /* root puede cambiar cualquier contraseña sin verificar la antigua */
    if (!current_user || current_user->uid != USERS_ROOT_UID) {
        if (!old_pwd || u->pwd_hash != djb2(old_pwd)) return -1;
    }
    u->pwd_hash = djb2(new_pwd);
    users_save();
    return 0;
}

/* ── Listado ──────────────────────────────────────────────── */

void users_list_all(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("UID    USUARIO          HOME\n");
    vga_puts("-----  ---------------  -------------------------\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    for (int i = 0; i < USERS_MAX; i++) {
        if (!users[i].used) continue;

        /* UID */
        vga_put_dec(users[i].uid);
        /* Padding uid */
        uint32_t uid_digits = users[i].uid == 0 ? 1 :
            (users[i].uid < 10 ? 1 : users[i].uid < 100 ? 2 :
             users[i].uid < 1000 ? 3 : users[i].uid < 10000 ? 4 : 5);
        for (uint32_t s = uid_digits; s < 7; s++) vga_putchar(' ');

        /* Nombre */
        vga_puts(users[i].name);
        int len = (int)strlen(users[i].name);
        for (int s = len; s < 17; s++) vga_putchar(' ');

        /* Home */
        vga_puts(users[i].home);

        /* Indicar usuario activo */
        if (current_user == &users[i]) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts("  *");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }
        vga_putchar('\n');
    }
}

/* ── Persistencia ATA ─────────────────────────────────────
 *
 * Diseño del disco (independiente del VFS):
 *   Sector 299 : cabecera de usuarios
 *   Sector 300 : volcado del array users[]
 */

#define USR_MAGIC   0x55535253U   /* "SRSU" */
#define USR_HDR_LBA 299U
#define USR_DAT_LBA 300U

typedef struct {
    uint32_t magic;
    uint32_t user_size;
    uint32_t user_count;
    uint32_t _pad[125];
} usr_disk_hdr_t;

void users_save(void) {
    if (!ata_present()) return;

    uint8_t buf[512];
    memset(buf, 0, 512);
    usr_disk_hdr_t* h = (usr_disk_hdr_t*)buf;
    h->magic      = USR_MAGIC;
    h->user_size  = sizeof(user_t);
    h->user_count = USERS_MAX;
    ata_write_sector(USR_HDR_LBA, buf);

    /* Escribir array: sizeof(user_t)*USERS_MAX bytes */
    const uint8_t* ptr = (const uint8_t*)users;
    uint32_t size = sizeof(users);
    uint32_t lba  = USR_DAT_LBA;
    while (size > 0) {
        uint32_t n = size < 512U ? size : 512U;
        memcpy(buf, ptr, n);
        if (n < 512U) memset(buf + n, 0, 512U - n);
        ata_write_sector(lba++, buf);
        ptr  += n;
        size -= n;
    }
}

int users_load(void) {
    if (!ata_present()) return 0;

    uint8_t buf[512];
    memset(buf, 0, 512);
    ata_read_sector(USR_HDR_LBA, buf);
    usr_disk_hdr_t* h = (usr_disk_hdr_t*)buf;

    if (h->magic      != USR_MAGIC)      return 0;
    if (h->user_size  != sizeof(user_t)) return 0;
    if (h->user_count != USERS_MAX)      return 0;

    uint8_t* ptr  = (uint8_t*)users;
    uint32_t size = sizeof(users);
    uint32_t lba  = USR_DAT_LBA;
    while (size > 0) {
        memset(buf, 0, 512);
        ata_read_sector(lba++, buf);
        uint32_t n = size < 512U ? size : 512U;
        memcpy(ptr, buf, n);
        ptr  += n;
        size -= n;
    }
    /* root no puede desaparecer nunca */
    if (!users[0].used || users[0].uid != USERS_ROOT_UID) {
        users[0].used     = 1;
        users[0].uid      = 0;
        users[0].pwd_hash = djb2("root");
        strncpy(users[0].name, "root",  USERS_NAME_LEN - 1);
        strncpy(users[0].home, "/root", USERS_HOME_LEN - 1);
    }
    return 1;
}
