#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#define TAM_BLOQUE (1024 * 1024) // 1. Tamaño de bloque de memoria es 1MB
#define TIEMPO_ESPERA 120


// Función para aumentar la memoria 
void aumentarMemoria(int n) {
    // Reserva n bloques de memoria 
	size_t total = (size_t)n * TAM_BLOQUE;
    // COMPLETAR USANDO MALLOC
	char *memoria = malloc(total);

    if (memoria == NULL) {
        fprintf(stderr, "Error al reservar memoria\n");
        exit(EXIT_FAILURE);
    }
    //COMPLETAR: ESCRIBIR EN MEMORIA USANDO MEMSET
	memset(memoria, 'D', total);
    printf(" [PID %d] Memoria aumentada exitosamente: %d MB\n", getpid(), n);
}

int main() {
    // Variable para almacenar la cantidad de subprocesos a crear
    int num_subprocesos;

	printf("PID del proceso padre: %d \n", getpid());
    printf("Ingrese el número de subprocesos a crear: \n");
	fflush(stdout); // Limpiamos la consola de salida estándar para leer
    scanf("%d", &num_subprocesos);

    // Bucle para crear el número especificado de subprocesos
    for (int i = 0; i < num_subprocesos; i++) {
        // COMPLETAR: Crear Subproceso
        pid_t pid = fork(); // FORK CREA PROCESO HIJO

        if (pid < 0) {
            // Error al crear el subproceso hijo
            fprintf(stderr, "Error al crear el subproceso hijo\n");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Proceso hijo
		printf("[Hijo %d] PID = %d, PPID= %d\n", i+1, getpid(), getppid());
		int n;
		printf("[PID %d] número de bloques de memoria a reservar (1 bloque = 1 MB):\n", getpid());
		fflush(stdout);
		scanf("%d", &n);
            // Completar: pedir un número de bloques de memoria al usuario y reservar
            //llamando a la función implementada más arriba
		aumentarMemoria(n);
            //COMPLETAR:
            exit(EXIT_SUCCESS);
        }
    }

    // Esperamos a que todos los subprocesos hijos terminen
    for (int i = 0; i < num_subprocesos; i++) {
        wait(NULL);
    }

    return EXIT_SUCCESS;
}

// COMANDO PARA COMPILAR:
// gcc -Wall -o fork1 fork.c

// EJECUTAR:
// ./fork1

// Crear 10 subprocesos
// Ver comportamiento de los procesos en la consola de Linux:
// ps -ef

// Ver arbol de procesos:
// pstree
