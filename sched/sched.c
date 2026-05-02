#include "sched.h"
#include "../memory/vmm.h"
#include "../memory/pmm.h"
#include "../drivers/vga.h"
#include "../libc/string.h"

/* ── Variables globales ───────────────────────────────────── */

static task_t tasks[MAX_TASKS];
static int    current_idx = 0;
static int    num_tasks   = 0;

/* Publico: permite habilitarlo desde kernel.c */
uint8_t sched_enabled = 0;

/* ── Punto de entrada de todas las nuevas tareas ─────────── */

static void task_starter(void) {
    /* Habilitar interrupciones en el nuevo contexto */
    __asm__ volatile("sti");

    /* Ejecutar la funcion de la tarea */
    tasks[current_idx].func();

    /* Si la funcion retorna, marcar como muerta */
    sched_exit();

    /* Nunca deberia llegar aqui */
    for (;;) __asm__ volatile("hlt");
}

/* ── Inicializacion ──────────────────────────────────────── */

void sched_init(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].pid         = (uint32_t)i;
        tasks[i].state       = TASK_DEAD;
        tasks[i].esp         = 0;
        tasks[i].stack       = (uint8_t*)0;
        tasks[i].func        = (void(*)(void))0;
        tasks[i].ticks       = 0;
        tasks[i].total_ticks = 0;
        tasks[i].name[0]     = '\0';
    }

    /* Tarea 0 = kernel/main: usa el stack de arranque */
    tasks[0].pid   = 0;
    tasks[0].state = TASK_RUNNING;
    tasks[0].stack = (uint8_t*)0;  /* stack de boot, no gestionado */
    tasks[0].func  = (void(*)(void))0;
    tasks[0].esp   = 0;            /* se rellenara en el primer switch */
    strncpy(tasks[0].name, "kernel", 31);
    tasks[0].name[31] = '\0';

    num_tasks   = 1;
    current_idx = 0;
    sched_enabled = 0;
}

/* ── Creacion de tareas ───────────────────────────────────── */

int sched_create(const char* name, void (*func)(void)) {
    /* Buscar TCB libre */
    int idx = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return -1;

    /* Asignar stack via VMM (frames físicos gestionados por PMM) */
    uint8_t* stack = (uint8_t*)vmm_alloc_pages(TASK_STACK / PAGE_SIZE);
    if (!stack) return -1;

    /* Inicializar TCB */
    tasks[idx].pid         = (uint32_t)idx;
    tasks[idx].state       = TASK_READY;
    tasks[idx].stack       = stack;
    tasks[idx].func        = func;
    tasks[idx].ticks       = 0;
    tasks[idx].total_ticks = 0;
    strncpy(tasks[idx].name, name, 31);
    tasks[idx].name[31] = '\0';

    /*
     * Configurar el stack inicial para que switch_context pueda
     * "retornar" a task_starter.
     *
     * Layout esperado en el stack al entrar en switch_context (pop edi/esi/ebx/ebp + ret):
     *   top-4:  EDI (= 0)
     *   top-8:  ESI (= 0)
     *   top-12: EBX (= 0)
     *   top-16: EBP (= 0)
     *   top-20: direccion de retorno -> task_starter
     *   top-24: (eventual arg de task_starter, no se usa)
     *
     * switch_context hace: pop edi, pop esi, pop ebx, pop ebp, ret
     * Entonces el ret saca la direccion de retorno = task_starter.
     */
    uint32_t* sp = (uint32_t*)(stack + TASK_STACK);

    /* Apilamos en orden inverso al que se desapilara */
    *(--sp) = (uint32_t)task_starter;  /* ret address -> task_starter */
    *(--sp) = 0;                        /* EBP */
    *(--sp) = 0;                        /* EBX */
    *(--sp) = 0;                        /* ESI */
    *(--sp) = 0;                        /* EDI */

    tasks[idx].esp = (uint32_t)sp;

    num_tasks++;
    return idx;
}

/* ── Limpieza de tareas muertas ───────────────────────────── */

/*
 * Libera el stack de cualquier tarea DEAD que no sea la actual.
 * Se llama al inicio de schedule() para que la memoria se recupere
 * en el siguiente ciclo tras la muerte de una tarea.
 */
static void sched_reap(void) {
    for (int i = 1; i < MAX_TASKS; i++) {
        if (i == current_idx) continue;        /* nunca liberar nuestro propio stack */
        if (tasks[i].state == TASK_DEAD && tasks[i].stack) {
            vmm_free_pages(tasks[i].stack, TASK_STACK / PAGE_SIZE);
            tasks[i].stack = (uint8_t*)0;
        }
    }
}

/* ── Planificador round-robin ─────────────────────────────── */

void schedule(void) {
    if (!sched_enabled) return;
    if (num_tasks <= 1) return;

    sched_reap();   /* liberar stacks de tareas ya muertas */

    int prev_idx = current_idx;

    /* Buscar la siguiente tarea READY (round-robin) */
    int next_idx = -1;
    for (int i = 1; i <= MAX_TASKS; i++) {
        int candidate = (current_idx + i) % MAX_TASKS;
        if (tasks[candidate].state == TASK_READY ||
            tasks[candidate].state == TASK_RUNNING) {
            next_idx = candidate;
            break;
        }
    }

    if (next_idx < 0 || next_idx == current_idx) return;

    /* Cambiar estado */
    if (tasks[prev_idx].state == TASK_RUNNING)
        tasks[prev_idx].state = TASK_READY;
    tasks[next_idx].state = TASK_RUNNING;

    current_idx = next_idx;

    /* Context switch */
    switch_context(&tasks[prev_idx].esp, tasks[next_idx].esp);
}

/* ── Tick del timer ──────────────────────────────────────── */

void sched_tick(void) {
    tasks[current_idx].total_ticks++;

    if (!sched_enabled || num_tasks <= 1) return;

    tasks[current_idx].ticks++;
    if (tasks[current_idx].ticks % SCHED_QUANTUM == 0)
        schedule();
}

/* ── Ceder la CPU voluntariamente ────────────────────────── */

void sched_yield(void) {
    __asm__ volatile("cli");
    schedule();
    __asm__ volatile("sti");
}

/* ── Bloquear la tarea actual ────────────────────────────── */

void sched_block(void) {
    __asm__ volatile("cli");
    tasks[current_idx].state = TASK_BLOCKED;
    schedule();
    __asm__ volatile("sti");
}

/* ── Desbloquear una tarea por PID ───────────────────────── */

void sched_unblock(uint32_t pid) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].pid == pid && tasks[i].state == TASK_BLOCKED) {
            tasks[i].state = TASK_READY;
            return;
        }
    }
}

/* ── Terminar la tarea actual ────────────────────────────── */

void sched_exit(void) {
    __asm__ volatile("cli");
    tasks[current_idx].state = TASK_DEAD;
    if (num_tasks > 0) num_tasks--;
    schedule();
    /* Si llegamos aqui no hay mas tareas: halt seguro */
    for (;;) __asm__ volatile("hlt");
}

/* ── Getters ─────────────────────────────────────────────── */

task_t* sched_current(void) {
    return &tasks[current_idx];
}

task_t* sched_get_task(int idx) {
    if (idx < 0 || idx >= MAX_TASKS) return (task_t*)0;
    return &tasks[idx];
}

int sched_num_tasks(void) {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_DEAD) count++;
    }
    return count;
}

/* ── Habilitar el planificador ───────────────────────────── */

void sched_start(void) {
    sched_enabled = 1;
}
