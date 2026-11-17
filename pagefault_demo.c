/*
 * pagefault_demo.c - Demand paging y page faults (U10 Post 1, Parte 2)
 * Reserva 100 MB con malloc, muestra VmSize/VmRSS antes y despues de
 * tocar cada pagina con memset.
 *
 * Nota: la version de la guia usaba system("grep ... /proc/self/status"),
 * pero system() lanza un proceso hijo (sh -> grep) y /proc/self apunta
 * al grep, no a este programa, por eso VmRSS salia igual antes y despues.
 * Aqui se lee /proc/self/status desde el mismo proceso con fopen.
 *
 * Compilar: gcc -o pagefault_demo pagefault_demo.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Imprime las lineas VmPeak, VmSize y VmRSS del propio proceso */
static void print_vm(const char *etiqueta) {
    char linea[256];
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) { perror("fopen"); return; }
    printf("--- %s ---\n", etiqueta);
    while (fgets(linea, sizeof linea, f))
        if (!strncmp(linea, "VmPeak", 6) || !strncmp(linea, "VmSize", 6) ||
            !strncmp(linea, "VmRSS", 5))
            fputs(linea, stdout);
    fclose(f);
}

int main(void) {
    size_t N = 100 * 1024 * 1024;  /* 100 MB */
    char *buf = (char *)malloc(N);
    if (!buf) { perror("malloc"); return 1; }

    /* Antes de tocar la memoria: reservada (VmSize) pero no residente */
    print_vm("Antes del memset");

    /* Forzar page faults escribiendo en cada pagina (100 MB / 4 KB = 25600) */
    memset(buf, 0xAA, N);

    /* Despues: RSS debe haber crecido ~100 MB */
    print_vm("Despues del memset");

    free(buf);
    return 0;
}
