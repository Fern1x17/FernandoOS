#include "vfs.h"
#include "../drivers/vga.h"
#include "../libc/string.h"

/* ── Estado global ────────────────────────────────────────── */

static vfs_node_t nodes[VFS_MAX_NODES];
static int current_dir = 0;   /* indice del directorio actual */

/* ── Auxiliares internas ──────────────────────────────────── */

/* Busca un nodo hijo de 'parent' con ese nombre */
static int find_node(int parent, const char* name) {
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (nodes[i].used && nodes[i].parent == parent
            && strcmp(nodes[i].name, name) == 0)
            return i;
    }
    return -1;
}

/* Devuelve el primer nodo libre */
static int find_free_node(void) {
    for (int i = 1; i < VFS_MAX_NODES; i++) {   /* 0 es la raiz */
        if (!nodes[i].used) return i;
    }
    return -1;
}

/*
 * Resuelve un path relativo o absoluto partiendo de un directorio base.
 * Devuelve el indice del ultimo directorio de la ruta, o -1 si no existe.
 * Si create_last es 1, crea el ultimo componente como directorio si no existe.
 */
static int resolve_path(const char* path, int* parent_out, char* last_component) {
    if (!path || path[0] == '\0') {
        if (parent_out) *parent_out = current_dir;
        if (last_component) last_component[0] = '\0';
        return current_dir;
    }

    int dir;
    const char* p;

    if (path[0] == '/') {
        dir = 0;   /* raiz */
        p = path + 1;
    } else {
        dir = current_dir;
        p = path;
    }

    /* Copiar path para poder tokenizarlo */
    char buf[128];
    strncpy(buf, p, 127);
    buf[127] = '\0';

    /* Dividir por '/' */
    char* tok = strtok(buf, "/");
    char* prev_tok = (char*)0;

    /* Si no hay tokens, la ruta era solo "/" */
    if (!tok) {
        if (parent_out) *parent_out = -1;
        if (last_component) last_component[0] = '\0';
        return dir;
    }

    /* Iterar todos los componentes excepto el ultimo */
    while (tok) {
        char* next = strtok((char*)0, "/");
        if (!next) {
            /* Es el ultimo componente */
            if (last_component) strncpy(last_component, tok, VFS_MAX_NAME - 1);
            if (parent_out) *parent_out = dir;
            return dir;
        }
        /* Es un componente intermedio: navegar */
        if (strcmp(tok, ".") == 0) {
            /* quedarse */
        } else if (strcmp(tok, "..") == 0) {
            if (dir != 0) dir = nodes[dir].parent;
        } else {
            int child = find_node(dir, tok);
            if (child < 0 || nodes[child].type != VFS_DIR) {
                if (parent_out) *parent_out = -1;
                if (last_component) last_component[0] = '\0';
                return -1;
            }
            dir = child;
        }
        tok = next;
        prev_tok = tok;
    }
    (void)prev_tok;
    if (parent_out) *parent_out = dir;
    if (last_component) last_component[0] = '\0';
    return dir;
}

/* ── Inicializacion ──────────────────────────────────────── */

void vfs_init(void) {
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        nodes[i].used   = 0;
        nodes[i].type   = 0;
        nodes[i].parent = -1;
        nodes[i].size   = 0;
        nodes[i].name[0] = '\0';
        nodes[i].data[0] = '\0';
    }
    /* Nodo 0 = directorio raiz */
    nodes[0].used   = 1;
    nodes[0].type   = VFS_DIR;
    nodes[0].parent = 0;   /* la raiz es su propio padre */
    nodes[0].size   = 0;
    strcpy(nodes[0].name, "/");

    current_dir = 0;
}

/* ── Operaciones de directorio ───────────────────────────── */

int vfs_mkdir(const char* path) {
    if (!path || path[0] == '\0') return -1;

    char last[VFS_MAX_NAME];
    int parent;
    resolve_path(path, &parent, last);

    if (parent < 0) {
        vga_puts("mkdir: ruta no valida\n");
        return -1;
    }
    if (last[0] == '\0') {
        vga_puts("mkdir: nombre vacio\n");
        return -1;
    }
    if (find_node(parent, last) >= 0) {
        vga_puts("mkdir: ya existe\n");
        return -1;
    }
    int idx = find_free_node();
    if (idx < 0) {
        vga_puts("mkdir: sin espacio\n");
        return -1;
    }

    nodes[idx].used   = 1;
    nodes[idx].type   = VFS_DIR;
    nodes[idx].parent = parent;
    nodes[idx].size   = 0;
    strncpy(nodes[idx].name, last, VFS_MAX_NAME - 1);
    nodes[idx].name[VFS_MAX_NAME - 1] = '\0';
    return idx;
}

int vfs_cd(const char* path) {
    if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
        current_dir = 0;
        return 0;
    }
    if (strcmp(path, ".") == 0) return 0;
    if (strcmp(path, "..") == 0) {
        if (current_dir != 0) current_dir = nodes[current_dir].parent;
        return 0;
    }

    /* Puede ser un path multi-componente */
    char last[VFS_MAX_NAME];
    int parent;
    int base = resolve_path(path, &parent, last);
    (void)base;

    int target;
    if (last[0] == '\0') {
        /* la ruta termina en '/' o es solo directorios intermedios */
        target = parent;
    } else {
        if (strcmp(last, "..") == 0) {
            target = (parent != 0) ? nodes[parent].parent : 0;
        } else if (strcmp(last, ".") == 0) {
            target = parent;
        } else {
            target = find_node(parent, last);
        }
    }

    if (target < 0) {
        vga_puts("cd: directorio no encontrado\n");
        return -1;
    }
    if (nodes[target].type != VFS_DIR) {
        vga_puts("cd: no es un directorio\n");
        return -1;
    }
    current_dir = target;
    return 0;
}

void vfs_pwd(void) {
    /* Reconstruir la ruta desde el nodo actual hasta la raiz */
    /* Guardar los indices en una pila pequeña */
    int stack[64];
    int top = 0;
    int idx = current_dir;

    while (idx != 0) {
        stack[top++] = idx;
        idx = nodes[idx].parent;
        if (top >= 64) break;
    }

    if (top == 0) {
        vga_puts("/\n");
        return;
    }

    for (int i = top - 1; i >= 0; i--) {
        vga_putchar('/');
        vga_puts(nodes[stack[i]].name);
    }
    vga_putchar('\n');
}

void vfs_ls(const char* path) {
    int dir;

    if (!path || path[0] == '\0' || strcmp(path, ".") == 0) {
        dir = current_dir;
    } else if (strcmp(path, "/") == 0) {
        dir = 0;
    } else {
        char last[VFS_MAX_NAME];
        int parent;
        resolve_path(path, &parent, last);

        if (last[0] == '\0') {
            dir = parent;
        } else {
            if (strcmp(last, "..") == 0)
                dir = (parent != 0) ? nodes[parent].parent : 0;
            else
                dir = find_node(parent, last);
        }
        if (dir < 0) {
            vga_puts("ls: directorio no encontrado\n");
            return;
        }
        if (nodes[dir].type != VFS_DIR) {
            vga_puts("ls: no es un directorio\n");
            return;
        }
    }

    int found = 0;
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used || nodes[i].parent != dir) continue;
        if (i == 0 && dir == 0) continue;  /* no mostrar la raiz como hijo de si misma */

        if (nodes[i].type == VFS_DIR) {
            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            vga_puts("[DIR] ");
        } else {
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_puts("[FIL] ");
        }
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_puts(nodes[i].name);
        if (nodes[i].type == VFS_FILE) {
            vga_puts("  (");
            vga_put_dec(nodes[i].size);
            vga_puts(" bytes)");
        }
        vga_putchar('\n');
        found++;
    }
    if (!found) {
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        vga_puts("(vacio)\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}

/* ── Operaciones de fichero ──────────────────────────────── */

int vfs_create(const char* path) {
    if (!path || path[0] == '\0') return -1;

    char last[VFS_MAX_NAME];
    int parent;
    resolve_path(path, &parent, last);

    if (parent < 0 || last[0] == '\0') {
        vga_puts("touch: ruta no valida\n");
        return -1;
    }
    if (find_node(parent, last) >= 0) {
        /* Ya existe: no error, comportamiento idempotente */
        return 0;
    }
    int idx = find_free_node();
    if (idx < 0) {
        vga_puts("touch: sin espacio\n");
        return -1;
    }

    nodes[idx].used   = 1;
    nodes[idx].type   = VFS_FILE;
    nodes[idx].parent = parent;
    nodes[idx].size   = 0;
    strncpy(nodes[idx].name, last, VFS_MAX_NAME - 1);
    nodes[idx].name[VFS_MAX_NAME - 1] = '\0';
    nodes[idx].data[0] = '\0';
    return idx;
}

int vfs_write(const char* path, const char* data, uint32_t len) {
    if (!path || path[0] == '\0') return -1;

    char last[VFS_MAX_NAME];
    int parent;
    resolve_path(path, &parent, last);

    int idx = (last[0] != '\0') ? find_node(parent, last) : -1;
    if (idx < 0) {
        vga_puts("write: fichero no encontrado\n");
        return -1;
    }
    if (nodes[idx].type != VFS_FILE) {
        vga_puts("write: no es un fichero\n");
        return -1;
    }
    if (len > VFS_MAX_DATA - 1) len = VFS_MAX_DATA - 1;

    for (uint32_t i = 0; i < len; i++)
        nodes[idx].data[i] = data[i];
    nodes[idx].data[len] = '\0';
    nodes[idx].size = len;
    return (int)len;
}

int vfs_read(const char* path, char* buf, uint32_t maxlen) {
    if (!path || path[0] == '\0') return -1;

    char last[VFS_MAX_NAME];
    int parent;
    resolve_path(path, &parent, last);

    int idx = (last[0] != '\0') ? find_node(parent, last) : -1;
    if (idx < 0) return -1;
    if (nodes[idx].type != VFS_FILE) return -1;

    uint32_t len = nodes[idx].size;
    if (len >= maxlen) len = maxlen - 1;
    for (uint32_t i = 0; i < len; i++)
        buf[i] = nodes[idx].data[i];
    buf[len] = '\0';
    return (int)len;
}

int vfs_rm(const char* name) {
    if (!name || name[0] == '\0') return -1;

    /* Buscar en el directorio actual */
    int idx = find_node(current_dir, name);
    if (idx < 0) {
        vga_puts("rm: no encontrado\n");
        return -1;
    }

    /* Si es un directorio, verificar que este vacio */
    if (nodes[idx].type == VFS_DIR) {
        for (int i = 0; i < VFS_MAX_NODES; i++) {
            if (nodes[i].used && nodes[i].parent == idx && i != idx) {
                vga_puts("rm: el directorio no esta vacio\n");
                return -1;
            }
        }
    }

    nodes[idx].used = 0;
    nodes[idx].name[0] = '\0';
    return 0;
}

void vfs_cat(const char* path) {
    if (!path || path[0] == '\0') {
        vga_puts("cat: nombre de fichero requerido\n");
        return;
    }

    char last[VFS_MAX_NAME];
    int parent;
    resolve_path(path, &parent, last);

    int idx = (last[0] != '\0') ? find_node(parent, last) : -1;
    if (idx < 0) {
        vga_puts("cat: fichero no encontrado\n");
        return;
    }
    if (nodes[idx].type != VFS_FILE) {
        vga_puts("cat: no es un fichero\n");
        return;
    }
    if (nodes[idx].size == 0) {
        vga_puts("(fichero vacio)\n");
        return;
    }
    vga_puts(nodes[idx].data);
    vga_putchar('\n');
}

/* ── Utilidad ────────────────────────────────────────────── */

int vfs_get_cwd(void) {
    return current_dir;
}
