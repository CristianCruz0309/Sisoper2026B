#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#define FIFO_FILE "canal_fifo"
#define SHM_KEY 1234
#define BUFFER_SIZE 128
#define TOTAL_DATOS 5

// Cristian Camilo Cruz Gallego
// Sebastián Orejuela Mina

typedef struct {
    int a;
    int b;
    int resultado_mul;
    int resultado_sum;
    int listo; // Variable de estado para sincronización: 0 (vacio), 1 (datos listos para sumar)
} DatosCompartidos;

int main() {
    int shm_id;
    DatosCompartidos *mem;
    pid_t pid_prod, pid_cons1, pid_cons2;

    // 1. Crear e inicializar memoria compartida
    shm_id = shmget(SHM_KEY, sizeof(DatosCompartidos), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Error creando memoria compartida");
        exit(1);
    }

    mem = (DatosCompartidos *) shmat(shm_id, NULL, 0);
    if (mem == (DatosCompartidos *) -1) {
        perror("Error asociando memoria compartida");
        exit(1);
    }
    mem->listo = 0; // Inicializar estado de sincronización

    // 2. Crear FIFO si no existe
    mkfifo(FIFO_FILE, 0666);

    // 3. Crear proceso Productor
    pid_prod = fork();
    if (pid_prod == 0) {
        int fd = open(FIFO_FILE, O_WRONLY);
        if (fd < 0) {
            perror("Error abriendo FIFO para escritura");
            exit(1);
        }

        for (int i = 1; i <= TOTAL_DATOS; i++) {
            int a = i, b = i + 2;
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "%d %d", a, b);
            write(fd, buffer, strlen(buffer) + 1);
            printf("[Productor] Enviado: %d y %d\n", a, b);
            sleep(1);
        }

        close(fd);
        exit(0);
    } 
    
    // 4. Crear Consumidor 1 (Multiplicación y orquestador del FIFO)
    pid_cons1 = fork();
    if (pid_cons1 == 0) {
        int *resultados_mul = (int *)malloc(TOTAL_DATOS * sizeof(int));
        int fd = open(FIFO_FILE, O_RDONLY);
        char buffer[BUFFER_SIZE];

        for (int i = 0; i < TOTAL_DATOS; i++) {
            // Esperar a que el consumidor 2 libere la memoria compartida
            while (mem->listo != 0) { usleep(10000); } 

            read(fd, buffer, BUFFER_SIZE);
            int a, b;
            sscanf(buffer, "%d %d", &a, &b);

            // Almacenar en memoria compartida
            mem->a = a;
            mem->b = b;
            
            // Calcular y guardar en arreglo dinámico
            resultados_mul[i] = a * b;
            mem->resultado_mul = resultados_mul[i];
            
            printf("[Consumidor 1] Leído del FIFO. %d * %d = %d\n", a, b, resultados_mul[i]);
            
            // Señalizar al Consumidor 2 que los datos están listos
            mem->listo = 1; 
        }

        close(fd);
        printf("[Consumidor 1] Arreglo final de multiplicaciones: ");
        for(int i=0; i<TOTAL_DATOS; i++) printf("%d ", resultados_mul[i]);
        printf("\n");
        free(resultados_mul);
        exit(0);
    } 
    
    // 5. Crear Consumidor 2 (Suma)
    pid_cons2 = fork();
    if (pid_cons2 == 0) {
        int *resultados_sum = (int *)malloc(TOTAL_DATOS * sizeof(int));
        
        for (int i = 0; i < TOTAL_DATOS; i++) {
            // Esperar activamente hasta que el Consumidor 1 escriba los datos
            while (mem->listo != 1) { usleep(10000); }

            // Leer de la memoria compartida (no del FIFO para evitar race conditions)
            int a = mem->a;
            int b = mem->b;
            
            // Calcular y guardar en arreglo dinámico
            resultados_sum[i] = a + b;
            mem->resultado_sum = resultados_sum[i];
            
            printf("[Consumidor 2] Leído de Memoria Compartida. %d + %d = %d\n", a, b, resultados_sum[i]);
            
            // Liberar la memoria compartida para la siguiente iteración
            mem->listo = 0; 
        }

        printf("[Consumidor 2] Arreglo final de sumas: ");
        for(int i=0; i<TOTAL_DATOS; i++) printf("%d ", resultados_sum[i]);
        printf("\n");
        free(resultados_sum);
        exit(0);
    }

    // 6. Proceso Padre: Esperar a los hijos y limpiar recursos
    waitpid(pid_prod, NULL, 0);
    waitpid(pid_cons1, NULL, 0);
    waitpid(pid_cons2, NULL, 0);

    printf("[Padre] Todos los procesos han terminado. Limpiando recursos...\n");
    
    // Desvincular y eliminar memoria compartida
    shmdt(mem);
    shmctl(shm_id, IPC_RMID, NULL);
    
    // Eliminar el FIFO
    unlink(FIFO_FILE);

    return 0;
}
