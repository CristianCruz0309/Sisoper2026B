//Cristian Camilo Cruz Gallego
//Sebastian Orejuela Mina
//Lab Corte 3 SisOper2026B

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

typedef struct {
    int pid, ppid;
    char nombre[64];
    char estado;
    long rss_kb;
    long cs_vol, cs_inv;
} InfoProc;

/* Lee /proc/<pid>/status y llena *p. Retorna 0 si pudo, -1 si el proceso ya no existe. */
static int leer_proceso(int pid, InfoProc *p)
{
    /* TODO 1: poner todo en cero (memset) y guardar el pid */
    memset(p, 0, sizeof *p);
    p->pid = pid;

    /* TODO 2: construir la ruta /proc/<pid>/status con snprintf y abrirla con fopen
       (si falla, retornar -1: el proceso pudo terminar) */
    char ruta[64];
    snprintf(ruta, sizeof ruta, "/proc/%d/status", pid);

    FILE *f = fopen(ruta, "r");
    if (!f) {
        return -1;
    }

    /* TODO 3: leer linea por linea (fgets) y extraer con sscanf los campos:
       Name, State, PPid, VmRSS, voluntary_ctxt_switches, nonvoluntary_ctxt_switches */
    char linea[256];
    while (fgets(linea, sizeof linea, f) != NULL) {
        if (sscanf(linea, "Name: %63s", p->nombre) == 1) continue;
        if (sscanf(linea, "State: %c", &p->estado) == 1) continue;
        if (sscanf(linea, "PPid: %d", &p->ppid) == 1) continue;
        if (sscanf(linea, "VmRSS: %ld", &p->rss_kb) == 1) continue;
        if (sscanf(linea, "voluntary_ctxt_switches: %ld", &p->cs_vol) == 1) continue;
        if (sscanf(linea, "nonvoluntary_ctxt_switches: %ld", &p->cs_inv) == 1) continue;
    }

    /* TODO 4: cerrar el archivo y retornar 0 */
    fclose(f);
    return 0;
}

int main(void)
{
    /* TODO 5: abrir /proc con opendir y verificar el retorno */
    DIR *d = opendir("/proc");
    if (!d) {
        perror("opendir /proc");
        return 1;
    }

    /* TODO 6: imprimir el encabezado; luego, por cada entrada (readdir) cuyo nombre
       empiece con digito, llamar a leer_proceso(atoi(nombre), &p) e imprimir una fila */
    printf("%6s %6s %-16s %c %9s %8s %8s\n",
           "PID", "PPID", "NOMBRE", 'E', "RSS(KiB)", "VOL", "INV");

    struct dirent *e;
    InfoProc p;
    int total = 0;
    long rss_total = 0;

    while ((e = readdir(d)) != NULL) {
        if (!isdigit((unsigned char)e->d_name[0])) {
            continue;
        }
        if (leer_proceso(atoi(e->d_name), &p) == -1) {
            continue;
        }

        printf("%6d %6d %-16s %c %9ld %8ld %8ld\n",
               p.pid, p.ppid, p.nombre, p.estado,
               p.rss_kb, p.cs_vol, p.cs_inv);

        /* TODO 7: acumular el total de procesos y de RSS */
        total++;
        rss_total += p.rss_kb;
    }

    /* TODO 7 (cont.): imprimir el resumen */
    printf("Procesos: %d | RSS total: %ld KiB\n", total, rss_total);

    /* TODO 8: cerrar el directorio */
    closedir(d);

    return 0;
}
