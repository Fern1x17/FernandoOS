#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_MAX_NODES  64
#define VFS_MAX_NAME   32
#define VFS_MAX_DATA   2048   /* bytes max por fichero */

#define VFS_FILE   1
#define VFS_DIR    2

typedef struct {
    char     name[VFS_MAX_NAME];
    uint8_t  type;
    int      used;
    int      parent;     /* indice del nodo padre */
    uint32_t size;
    char     data[VFS_MAX_DATA];
} vfs_node_t;

void vfs_init(void);

/* Operaciones de directorio */
int  vfs_mkdir(const char* path);
int  vfs_cd(const char* path);
void vfs_pwd(void);
void vfs_ls(const char* path);   /* path="" o "." = cwd */

/* Operaciones de fichero */
int  vfs_create(const char* path);
int  vfs_write(const char* path, const char* data, uint32_t len);
int  vfs_read(const char* path, char* buf, uint32_t maxlen);
int  vfs_rm(const char* name);
void vfs_cat(const char* path);

/* Utilidad */
int vfs_get_cwd(void);

#endif
