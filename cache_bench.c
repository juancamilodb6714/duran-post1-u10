/*
 * cache_bench.c - Benchmark de jerarquia de cache (U10 Post 1, Parte 1)
 * Mide la latencia de acceso a memoria para arrays de distintos tamanos
 * con tres patrones: secuencial, aleatorio (Fisher-Yates) y stride fijo.
 * Compilar: gcc -O0 -o cache_bench cache_bench.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define REPEAT 100

#define KB (1024UL)
#define MB (1024UL * 1024UL)

/* Experimento de stride: array de 1 MB (cabe completo en la L2 de 2 MB) */
#define STRIDE_ARRAY  (1 * MB)
#define STRIDE_LINEA  64UL     /* tamano de una linea de cache */
#define STRIDE_PAGINA 4096UL   /* L1d 48 KB / 12 vias = 4096 B */

/* Medir tiempo de acceso secuencial a array de N bytes */
double bench_seq(size_t n_bytes) {
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr) return -1.0;
    memset((void*)arr, 1, n_bytes);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int r = 0; r < REPEAT; r++)
        for (size_t i = 0; i < n_bytes; i++)
            (void)arr[i];   /* acceso de lectura */

    clock_gettime(CLOCK_MONOTONIC, &t1);
    free((void*)arr);

    double ns = (t1.tv_sec - t0.tv_sec)*1e9
               + (t1.tv_nsec - t0.tv_nsec);
    return ns / (REPEAT * (double)n_bytes);  /* ns/byte */
}

/* Acceso aleatorio usando indices pre-mezclados (Fisher-Yates) */
double bench_rand(size_t n_bytes) {
    size_t n = n_bytes / sizeof(int);
    int *arr = (int *)malloc(n * sizeof(int));
    size_t *idx = (size_t *)malloc(n * sizeof(size_t));
    if (!arr || !idx) { free(arr); free(idx); return -1.0; }

    for (size_t i = 0; i < n; i++) { arr[i] = (int)i; idx[i] = i; }
    /* Mezcla Fisher-Yates */
    for (size_t i = n-1; i > 0; i--) {
        size_t j = (size_t)rand() % (i+1);
        size_t tmp = idx[i]; idx[i] = idx[j]; idx[j] = tmp;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    volatile long sum = 0;
    for (int r = 0; r < REPEAT; r++)
        for (size_t i = 0; i < n; i++)
            sum += arr[idx[i]];
    clock_gettime(CLOCK_MONOTONIC, &t1);

    free(arr); free(idx);
    double ns = (t1.tv_sec-t0.tv_sec)*1e9+(t1.tv_nsec-t0.tv_nsec);
    return ns/(REPEAT*(double)n);   /* ns/acceso */
}

/* Acceso con stride fijo: expone conflictos de asociatividad */
double bench_stride(size_t n_bytes, size_t stride) {
    volatile char *arr = (volatile char *)malloc(n_bytes);
    if (!arr) return -1.0;
    memset((void*)arr, 1, n_bytes);

    size_t steps = n_bytes / stride;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int r = 0; r < REPEAT; r++)
        for (size_t i = 0; i < steps; i++)
            (void)arr[(i * stride) % n_bytes];

    clock_gettime(CLOCK_MONOTONIC, &t1);
    free((void*)arr);

    double ns = (t1.tv_sec - t0.tv_sec)*1e9 + (t1.tv_nsec - t0.tv_nsec);
    return ns / (REPEAT * (double)steps);   /* ns/acceso */
}

/*
 * Complemento del Paso 4: persecucion de punteros (pointer chasing).
 * Recorre n_lines lineas separadas por 'stride' bytes, donde cada linea
 * guarda la direccion de la siguiente. Cada carga depende de la anterior,
 * asi que ni la ejecucion fuera de orden ni el prefetcher esconden la
 * latencia. Con stride 4096 todas las lineas caen en el mismo set de L1;
 * si n_lines supera las 12 vias de la L1d aparecen conflict misses.
 */
#define CHASE_TOTAL 5000000L
double bench_conflict(size_t n_lines, size_t stride) {
    size_t n_bytes = n_lines * stride;
    char *arr = (char *)aligned_alloc(4096, n_bytes < 4096 ? 4096 : n_bytes);
    if (!arr) return -1.0;
    memset(arr, 0, n_bytes);
    for (size_t i = 0; i < n_lines; i++)
        *(char **)(arr + i*stride) = arr + ((i+1) % n_lines) * stride;

    char **p = (char **)arr;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (long k = 0; k < CHASE_TOTAL; k++)
        p = (char **)*(char * volatile *)p;   /* volatile: -O2 no la elimina */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    volatile void *sink = p; (void)sink;
    free(arr);

    double ns = (t1.tv_sec - t0.tv_sec)*1e9 + (t1.tv_nsec - t0.tv_nsec);
    return ns / (double)CHASE_TOTAL;   /* ns/acceso dependiente */
}

int main(void) {
    size_t sizes[] = {
        4*KB, 8*KB, 16*KB, 32*KB,      /* region L1  (L1d = 48 KB)  */
        64*KB, 128*KB, 256*KB,         /* region L2  (L2  = 2 MB)   */
        512*KB, 1*MB, 4*MB,            /* region L2/L3              */
        16*MB, 64*MB                   /* region L3/RAM             */
    };
    int n = sizeof(sizes)/sizeof(sizes[0]);

    printf("== Acceso secuencial vs aleatorio ==\n");
    printf("%-12s  %12s  %14s  %8s\n",
           "Array (KB)", "seq ns/byte", "rand ns/acceso", "rand/seq");
    for (int i = 0; i < n; i++) {
        double lat_seq  = bench_seq(sizes[i]);
        double lat_rand = bench_rand(sizes[i]);
        printf("%-12zu  %12.3f  %14.3f  %8.2f\n",
               sizes[i]/1024, lat_seq, lat_rand, lat_rand/lat_seq);
        fflush(stdout);
    }

    printf("\n== Stride sobre array de %lu KB ==\n", STRIDE_ARRAY/KB);
    printf("%-12s  %10s\n","Stride","ns/acceso");
    double t_line = bench_stride(STRIDE_ARRAY, STRIDE_LINEA);
    double t_page = bench_stride(STRIDE_ARRAY, STRIDE_PAGINA);
    printf("%-12s  %10.3f\n", "64 B", t_line);
    printf("%-12s  %10.3f\n", "4096 B", t_page);
    printf("Relacion 4096/64: %.2fx\n", t_page / t_line);

    printf("\n== Conflictos de asociatividad (pointer chasing, L1d 12 vias) ==\n");
    printf("%-8s  %10s  %10s  %10s\n", "Lineas", "64 B", "2048 B", "4096 B");
    size_t lines[] = {8, 12, 16, 24, 32};
    for (int i = 0; i < 5; i++)
        printf("%-8zu  %10.3f  %10.3f  %10.3f\n", lines[i],
               bench_conflict(lines[i], STRIDE_LINEA),
               bench_conflict(lines[i], 2048),
               bench_conflict(lines[i], STRIDE_PAGINA));
    return 0;
}
