
/*
        EJERCICIO DE EXTENSION - MANEJO DE MEMORIA DINAMICA

        Funcionalidades implementadas hasta ahora:

        1. Arreglo dinámico
        2. Crecimiento mediante realloc
        3. Inserción de elementos
        4. Eliminación de elementos
        5. Reducción de capacidad (shrink-to-fit)
        6. Arreglo dinámico genérico usando void *
        7. Prueba con int
*/

#include <stdio.h>
#include <stdlib.h>

// ============================================================
// ESTRUCTURA DEL ARREGLO DINAMICO GENERICO
// ============================================================

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int *datos;
    size_t size;      /* Elementos actualmente usados */
    size_t capacity;  /* Capacidad total reservada */
} ArregloDinamico;

void arreglo_iniciar(ArregloDinamico *arr, size_t capacidad_inicial) {
    if (capacidad_inicial == 0) capacidad_inicial = 1;
    arr->datos = (int *) malloc(capacidad_inicial * sizeof(int));
    if (arr->datos == NULL) {
        fprintf(stderr, "Error: malloc no pudo reservar memoria.\n");
        exit(EXIT_FAILURE);
    }
    arr->size = 0;
    arr->capacity = capacidad_inicial;
    printf("[INFO] Arreglo iniciado con capacidad %zu.\n", capacidad_inicial);
}

void arreglo_agregar(ArregloDinamico *arr, int valor) {
    if (arr->size == arr->capacity) {
        size_t nueva_capacidad = arr->capacity * 2;
        int *tmp = (int *) realloc(arr->datos, nueva_capacidad * sizeof(int));
        if (tmp == NULL) {
            fprintf(stderr, "Error: realloc no pudo ampliar la memoria.\n");
            free(arr->datos);
            exit(EXIT_FAILURE);
        }
        arr->datos = tmp;
        arr->capacity = nueva_capacidad;
        printf("[INFO] Realloc duplicar: %zu elementos.\n", arr->capacity);
    }
    arr->datos[arr->size] = valor;
    arr->size++;
}

void arreglo_insertar(ArregloDinamico *arr, size_t indice, int valor) {
    if (indice > arr->size) {
        fprintf(stderr, "Error: Indice %zu fuera de rango (size = %zu).\n", indice, arr->size);
        return;
    }

    /* Redimensionar si está lleno */
    if (arr->size == arr->capacity) {
        size_t nueva_capacidad = arr->capacity * 2;
        int *tmp = (int *) realloc(arr->datos, nueva_capacidad * sizeof(int));
        if (tmp == NULL) {
            fprintf(stderr, "Error: realloc fallo en insertar.\n");
            free(arr->datos);
            exit(EXIT_FAILURE);
        }
        arr->datos = tmp;
        arr->capacity = nueva_capacidad;
        printf("[INFO] Insertar requirio realloc: nueva capacidad = %zu.\n", arr->capacity);
    }

    /* Desplazar elementos a la derecha desde el final hasta el índice */
    for (size_t i = arr->size; i > indice; i--) {
        arr->datos[i] = arr->datos[i - 1];
    }

    arr->datos[indice] = valor;
    arr->size++;
}

void arreglo_eliminar(ArregloDinamico *arr, size_t indice) {
    if (indice >= arr->size) {
        fprintf(stderr, "Error: Indice %zu fuera de rango (size = %zu).\n", indice, arr->size);
        return;
    }

    /* Desplazar elementos a la izquierda para cubrir la vacante */
    for (size_t i = indice; i < arr->size - 1; i++) {
        arr->datos[i] = arr->datos[i + 1];
    }

    arr->size--;

    /* Shrink-to-fit: Si size baja del 25% de la capacidad */
    if (arr->size > 0 && arr->size <= arr->capacity / 4) {
        size_t nueva_capacidad = arr->capacity / 2;
        
        /* Evita reducir demasiado si la capacidad inicial era pequeña */
        if (nueva_capacidad >= 2) {
            printf("[INFO] Shrink-to-fit: size (%zu) <= 25%% capacity (%zu). Reduciendo a %zu...\n",
                   arr->size, arr->capacity, nueva_capacidad);

            int *tmp = (int *) realloc(arr->datos, nueva_capacidad * sizeof(int));
            if (tmp != NULL) {
                arr->datos = tmp;
                arr->capacity = nueva_capacidad;
            }
        }
    }
}

long arreglo_buscar(const ArregloDinamico *arr, int valor) {
    for (size_t i = 0; i < arr->size; i++) {
        if (arr->datos[i] == valor) {
            return (long)i;
        }
    }
    return -1;
}

void arreglo_imprimir(const ArregloDinamico *arr) {
    printf("Arreglo [size=%zu, cap=%zu]: ", arr->size, arr->capacity);
    for (size_t i = 0; i < arr->size; i++) {
        printf("%d ", arr->datos[i]);
    }
    printf("\n");
}

void arreglo_liberar(ArregloDinamico *arr) {
    free(arr->datos);
    arr->datos = NULL;
    arr->size = 0;
    arr->capacity = 0;
    printf("[INFO] Memoria liberada correctamente.\n");
}

int main(void) {
    ArregloDinamico arr;
    arreglo_iniciar(&arr, 4);

    /* Llenar el arreglo */
    for (int i = 1; i <= 8; i++) {
        arreglo_agregar(&arr, i * 10);
    }
    arreglo_imprimir(&arr);

    /* Probando inserción en el medio */
    printf("\n--- Probando insercion en indice 3 (valor 999) ---\n");
    arreglo_insertar(&arr, 3, 999);
    arreglo_imprimir(&arr);

    /* Probando búsqueda */
    printf("\n--- Probando busqueda ---\n");
    long idx = arreglo_buscar(&arr, 999);
    printf("Busqueda de 999: indice %ld\n", idx);
    printf("Busqueda de 500 (inexistente): indice %ld\n", arreglo_buscar(&arr, 500));

    /* Probando eliminación y shrink-to-fit */
    printf("\n--- Probando eliminacion masiva para disparar shrink ---\n");
    while (arr.size > 1) {
        printf("Eliminando indice 0 (valor %d)...\n", arr.datos[0]);
        arreglo_eliminar(&arr, 0);
        arreglo_imprimir(&arr);
    }

    arreglo_liberar(&arr);
    return 0;
}
