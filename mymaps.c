/*
 * mymaps.c - Mapa de memoria virtual de un proceso (U10 Post 1, Parte 2)
 * Imprime direcciones de stack, heap y datos globales y se queda
 * esperando un Enter para poder leer /proc/PID/maps mientras vive.
 * Compilar: gcc -o mymaps mymaps.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int global_data = 42;          /* segmento .data */
const char *msg = "hola";      /* segmento .rodata */

int main(void) {
    void *heap = malloc(1024 * 1024);  /* 1 MB en heap */
    printf("PID: %d\n", getpid());
    printf("stack var addr: %p\n", (void*)&heap);
    printf("heap addr:      %p\n", heap);
    printf("global addr:    %p\n", (void*)&global_data);
    printf("rodata addr:    %p\n", (void*)msg);
    printf("Presione Enter para continuar...\n");
    fflush(stdout);
    getchar();
    free(heap);
    return 0;
}
