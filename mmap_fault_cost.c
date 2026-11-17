/*
 * mmap_fault_cost.c - Costo de un minor page fault (U10 Post 1, Parte 2)
 * Mide el primer toque (con fault) y el segundo toque (sin fault)
 * de 200 paginas de un mmap anonimo.
 * Compilar: gcc -o mmap_fault_cost mmap_fault_cost.c
 * Variante para la decision tecnica del Paso 6 (MAP_POPULATE):
 *   gcc -DUSE_POPULATE -o mmap_populate mmap_fault_cost.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>

#define N_PAGES 200

#ifdef USE_POPULATE
#define EXTRA_FLAGS MAP_POPULATE   /* el kernel llena las tablas de paginas en mmap */
#else
#define EXTRA_FLAGS 0              /* demand paging por defecto */
#endif

int main(void) {
    long page_size = sysconf(_SC_PAGESIZE);
    size_t total = (size_t)N_PAGES * page_size;

    struct timespec t0, t1;

    /* mmap anonimo: reserva direcciones virtuales sin RAM fisica asociada */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    char *base = mmap(NULL, total, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | EXTRA_FLAGS, -1, 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (base == MAP_FAILED) { perror("mmap"); return 1; }
    double t_mmap = (t1.tv_sec - t0.tv_sec)*1e9 + (t1.tv_nsec - t0.tv_nsec);
    double sum_first = 0, sum_second = 0;

    for (int i = 0; i < N_PAGES; i++) {
        char *page = base + (size_t)i * page_size;

        /* Primer toque: la pagina aun no tiene marco fisico -> minor page fault */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        page[0] = 1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        sum_first += (t1.tv_sec - t0.tv_sec)*1e9 + (t1.tv_nsec - t0.tv_nsec);

        /* Segundo toque: la pagina ya esta mapeada, no hay fault */
        clock_gettime(CLOCK_MONOTONIC, &t0);
        page[0] = 2;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        sum_second += (t1.tv_sec - t0.tv_sec)*1e9 + (t1.tv_nsec - t0.tv_nsec);
    }

    printf("Modo: %s\n", EXTRA_FLAGS ? "MAP_POPULATE" : "demand paging (por defecto)");
    printf("Tamano de pagina: %ld bytes, paginas: %d\n", page_size, N_PAGES);
    printf("Tiempo de la llamada mmap:                %.1f ns\n", t_mmap);
    printf("Costo promedio primer toque (con fault):  %.1f ns\n", sum_first / N_PAGES);
    printf("Costo promedio segundo toque (sin fault): %.1f ns\n", sum_second / N_PAGES);
    printf("Overhead atribuible al page fault:        %.1f ns\n",
           (sum_first - sum_second) / N_PAGES);

    munmap(base, total);
    return 0;
}
