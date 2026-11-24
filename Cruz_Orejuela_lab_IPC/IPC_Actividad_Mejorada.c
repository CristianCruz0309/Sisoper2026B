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
#include <semaphore.h> // Necesario para semáforos POSIX

#define FIFO_FILE "canal_fifo"
#define SHM_KEY 1234
#define BUFFER_SIZE 128
#define TOTAL_DATOS 5

// Estructura actualizada con semáforos y bandera de finalización
typedef struct {
    int a;
    int b;
    int resultado_mul;
    int resultado_sum;
    int fin;        // 1 = no habrá más pares, el productor terminó
    sem_t vacio;    // 1 = el buffer está libre para escribir
    sem_t lleno;    // 1 = hay un par disponible para leer
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

    // 2. Inicializar semáforos POSIX en memoria compartida (pshared = 1) y variables
    memset(mem, 0, sizeof(*mem));
    sem_init(&mem->vacio, 1, 1); 
    sem_init(&mem->lleno, 1, 0);
    mem->fin = 0;

    // 3. Crear FIFO si no existe
    mkfifo(FIFO_FILE, 0666);

    // 4. Crear proceso Productor
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
            
            // Envía la cadena con su caracter nulo terminador (\0)
            snprintf(buffer, BUFFER_SIZE, "%d %d", a, b);
            write(fd, buffer, strlen(buffer) + 1);
            printf("[Productor] Enviado: %d y %d\n", a, b);
            sleep(1);
        }

        close(fd); // Al cerrar el FIFO, los lectores recibirán un read() = 0 (EOF)
        exit(0);
    } 
    
    // 5. Crear Consumidor 1 (Multiplicación y orquestador del FIFO)
    pid_cons1 = fork();
    if (pid_cons1 == 0) {
        int *resultados_mul = (int *)malloc(TOTAL_DATOS * sizeof(int));
        int n = 0; // Índice para el arreglo dinámico
        
        int fd = open(FIFO_FILE, O_RDONLY);
        char buffer[BUFFER_SIZE];
        ssize_t bytes_leidos;

        // Lectura robusta: procesa el FIFO hasta que el productor lo cierre (read devuelve 0)
        while ((bytes_leidos = read(fd, buffer, BUFFER_SIZE)) > 0) {
            // Recorre los mensajes leídos (por si llegaron varios concatenados)
            for (char *p = buffer; p < buffer + bytes_leidos; p += strlen(p) + 1) {
                int a, b;
                if (sscanf(p, "%d %d", &a, &b) == 2) {
                    
                    // Sección Crítica: Publicar en Memoria Compartida
                    sem_wait(&mem->vacio); // Espera a que esté libre
                    mem->a = a;
                    mem->b = b;
                    sem_post(&mem->lleno); // Avisa al Consumidor 2 que hay datos
                    
                    // Cálculo local
                    resultados_mul[n++] = a * b;
                    printf("[Consumidor 1] Leído del FIFO. %d * %d = %d\n", a, b, resultados_mul[n-1]);
                }
            }
        }
        close(fd);

        // Notificar el final al Consumidor 2
        sem_wait(&mem->vacio);
        mem->fin = 1;
        sem_post(&mem->lleno);

        printf("[Consumidor 1] Arreglo final de multiplicaciones: ");
        for(int i = 0; i < n; i++) printf("%d ", resultados_mul[i]);
        printf("\n");
        
        free(resultados_mul);
        exit(0);
    } 
    
    // 6. Crear Consumidor 2 (Suma)
    pid_cons2 = fork();
    if (pid_cons2 == 0) {
        int *resultados_sum = (int *)malloc(TOTAL_DATOS * sizeof(int));
        int n = 0;

        for (;;) {
            // Sección Crítica: Leer de Memoria Compartida
            sem_wait(&mem->lleno); // Se bloquea pasivamente hasta que haya datos
            
            if (mem->fin == 1) {
                sem_post(&mem->vacio); // Libera el semáforo por buenas prácticas antes de salir
                break;
            }

            int a = mem->a;
            int b = mem->b;
            sem_post(&mem->vacio); // Libera el espacio para el Consumidor 1
            
            // Cálculo local
            resultados_sum[n++] = a + b;
            printf("[Consumidor 2] Leído de Memoria Compartida. %d + %d = %d\n", a, b, resultados_sum[n-1]);
        }

        printf("[Consumidor 2] Arreglo final de sumas: ");
        for(int i = 0; i < n; i++) printf("%d ", resultados_sum[i]);
        printf("\n");
        
        free(resultados_sum);
        exit(0);
    }

    // 7. Proceso Padre: Esperar a los hijos y limpiar recursos
    waitpid(pid_prod, NULL, 0);
    waitpid(pid_cons1, NULL, 0);
    waitpid(pid_cons2, NULL, 0);

    printf("[Padre] Todos los procesos han terminado. Limpiando recursos...\n");
    
    // Destruir los semáforos
    sem_destroy(&mem->vacio);
    sem_destroy(&mem->lleno);

    // Desvincular y eliminar memoria compartida
    shmdt(mem);
    shmctl(shm_id, IPC_RMID, NULL);
    
    // Eliminar el FIFO
    unlink(FIFO_FILE);

    return 0;
}
