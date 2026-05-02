#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

#define MAX_TASKS     16
#define TASK_STACK    8192   /* 8 KiB por tarea */
#define SCHED_QUANTUM 10     /* ticks entre preempciones */

typedef enum {
    TASK_RUNNING = 0,
    TASK_READY   = 1,
    TASK_BLOCKED = 2,
    TASK_DEAD    = 3,
} task_state_t;

typedef struct {
    uint32_t     pid;
    char         name[32];
    task_state_t state;
    uint32_t     esp;        /* ESP guardado en context switch */
    uint8_t*     stack;      /* memoria del stack */
    void       (*func)(void);
    uint32_t     ticks;      /* ticks de quantum actual */
    uint32_t     total_ticks;/* ticks de CPU totales consumidos */
} task_t;

/* Variable global publica para habilitar/deshabilitar el planificador */
extern uint8_t sched_enabled;

void     sched_init(void);
int      sched_create(const char* name, void (*func)(void));
void     sched_tick(void);            /* llamado desde timer IRQ */
void     sched_yield(void);           /* cede voluntariamente la CPU */
void     sched_block(void);           /* bloquea la tarea actual */
void     sched_unblock(uint32_t pid); /* desbloquea una tarea por PID */
void     sched_exit(void);            /* termina la tarea actual */
task_t*  sched_current(void);         /* devuelve la tarea actual */
task_t*  sched_get_task(int idx);     /* acceso por indice */
int      sched_num_tasks(void);       /* numero de tareas vivas */
void     schedule(void);              /* ejecuta el planificador */
void     sched_start(void);           /* habilita el planificador */

/* Declarado en switch.asm */
extern void switch_context(uint32_t* old_esp_ptr, uint32_t new_esp);

#endif
