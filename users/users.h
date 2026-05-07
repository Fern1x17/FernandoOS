#ifndef USERS_H
#define USERS_H

#include <stdint.h>

#define USERS_MAX       8
#define USERS_NAME_LEN  16
#define USERS_HOME_LEN  32
#define USERS_ROOT_UID  0

typedef struct {
    int      used;
    uint32_t uid;
    char     name[USERS_NAME_LEN];
    uint32_t pwd_hash;
    char     home[USERS_HOME_LEN];
} user_t;

void    users_init(void);

/* Autenticación */
int     users_login(const char* name, const char* password);
void    users_logout(void);
void    users_switch(const char* name);   /* cambia sin contraseña (uso interno/root) */

/* Consulta */
user_t* users_current(void);
user_t* users_get_by_name(const char* name);
user_t* users_get_by_uid(uint32_t uid);
int     users_count(void);

/* Gestión */
int     users_add(const char* name, const char* password, uint32_t uid);
int     users_del(const char* name);
int     users_passwd(const char* name, const char* old_pwd, const char* new_pwd);

/* Listado */
void    users_list_all(void);

/* Persistencia ATA */
void    users_save(void);
int     users_load(void);   /* 0 = disco virgen/error, 1 = cargado */

#endif
