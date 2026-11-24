# Sistemas Operativos - Universidad Santiago de Cali

Resumen de estudio · Segundo corte · Procesos, IPC, sincronización, bloqueos y planificación vistos desde el laboratorio Productor-Consumidores

---

## 1. El proceso como unidad de gestión de recursos

### PCB: la visión del SO
Un **proceso** es un programa en ejecución. El SO no ve "código": ve una estructura de datos por proceso, el **Bloque de Control de Proceso (PCB)**, que le permite suspender y reanudar la ejecución (cambio de contexto).

* **Identificación:** PID, PPID (padre), usuario.
* **Estado del procesador:** contador de programa, registros, banderas (se guardan al expulsarlo).
* **Control:** estado, prioridad, datos de planificación (`vruntime`, quantum), punteros a colas.
* **Recursos:** tabla de descriptores abiertos (FIFO, `shm`), tablas de memoria, señales pendientes.

En Linux: `task_struct`, visible en `/proc/<PID>/status`, `/proc/<PID>/fd`.

### Process Image: la visión del usuario
Es el conjunto de lo que el proceso "es" en memoria: **código (text)**, **datos** (globales), **heap** (`malloc`), **pila** (variables locales) y el propio **PCB**. Cada proceso tiene su espacio de direcciones aislado.

> **En el laboratorio:** tras `fork` cada hijo recibe su copia (copy-on-write) del heap. Por eso el arreglo de resultados creado con `malloc` es **privado**: el Consumidor 1 no ve el arreglo del 2. Solo el segmento `shmget/shmat` es compartido, y por eso es el único lugar donde aparecen carreras.

### Ciclo de vida y estados

| Estado | Significado | Dónde aparece en el laboratorio | Código `ps` |
| :--- | :--- | :--- | :--- |
| Nuevo | PCB creado por `fork`, aún sin admitir | Instante de crear Productor y Consumidores | - |
| Listo | Espera CPU en la cola de listos | Los tres hijos tras el `fork` | `R` |
| Ejecución | Usa la CPU | Cálculo de $a \times b$, $a + b$; espera activa | `R` |
| Bloqueado | Espera un evento (E/S, semáforo, tiempo) | `read` en FIFO vacío (`pipe_read`), `sem_wait` (`futex`), `sleep(1)` del productor | `S` |
| Terminado / zombi | Ejecutó `exit`; el PCB persiste hasta que el padre haga `wait` | Padre sin `wait()` (P9) | `Z` |

**Cambio de contexto:** guardar el estado del proceso saliente en su PCB y cargar el del entrante. *Voluntario* cuando el proceso se bloquea; *no voluntario* cuando el planificador lo expulsa (típico de la espera activa). Se mide con `grep ctxt /proc/<PID>/status` y `pidstat -w`.

---

## 2. El laboratorio en una página

**Problema:** un Productor genera pares `(a, b)` (uno por segundo) y los envía por un **FIFO**; dos Consumidores colaboran: uno calcula $a \times b$ y otro $a + b$, usando **memoria compartida** y arreglos `malloc`. Exigencias: no perder ni sobrescribir datos, esperar correctamente y liberar recursos.

```text
PRODUCTOR --write--> [FIFO canal_fifo] --read--> CONSUMIDOR 1 (a*b)
                                                    | publica (a,b)
                                                    v
                                      [SHM: a | b | mul | sum | fin | vacio | lleno]
                                                    | lee (a,b)
                                                    v
                                              CONSUMIDOR 2 (a+b)
malloc(): arreglos de resultados PRIVADOS de cada proceso
```

**Decisión de diseño (Opción A):** solo el Consumidor 1 lee el FIFO y publica cada par en `shm`; el Consumidor 2 lo toma de allí. Se descartan B (un FIFO por consumidor) y C (ambos compiten por el FIFO, que solo reparte trabajo) porque la A exige sincronización real y es la más rica didácticamente.

| Problema del código base | Causa | Solución |
| :--- | :--- | :--- |
| P1 Lectores compiten por el FIFO | Un FIFO entrega cada byte a *un solo* lector (cola de trabajo, no difusión) | Un único lector (Opción A) |
| P2 Sobrescritura de `a, b` | Un solo espacio compartido sin control | Semáforos `vacio`/`lleno` |
| P3 `listo` no atómico | Verificar y modificar son pasos separados | `sem_wait` es atómico |
| P4 Espera activa | `while(!listo);` consume ~100 % CPU | Espera bloqueante (estado `S`) |
| P5 Sin visibilidad | Con `-O2` el compilador deja `listo` en un registro | Los semáforos actúan como barrera de memoria |
| P6 FIFO = flujo de bytes | Un `read` puede traer dos mensajes juntos | Recorrer el búfer por `\0`, o mensajes de tamaño fijo |
| P7 `sleep` como sincronización | Solo funciona si se cumplen supuestos de tiempo | Sincronizar por eventos |
| P8 Sin fin de datos | El Consumidor 2 no sabe que el FIFO se cerró | Campo `fin` + `sem_post(lleno)` |
| P9 Zombis | Padre sin `wait` | 3 × `wait(NULL)` |
| P10 Recursos huérfanos | Clave fija 1234; `shm` y FIFO persisten | `shmctl(IPC_RMID)`, `unlink`, `ftok`/`IPC_PRIVATE` |
| P11 Fuga de memoria | `malloc` sin `free` | `free` antes de `exit`; `valgrind` |

### Solución: buffer de un elemento

```c
/* Cons. 1: por cada par leído del FIFO */
sem_wait(&mem->vacio);   /* ¿ya tomó el 2 el par previo? */
mem->a = a; mem->b = b;
sem_post(&mem->lleno);   /* hay par nuevo */
res_mul[n++] = a * b;
/* read == 0: */
sem_wait(&mem->vacio); mem->fin = 1;
sem_post(&mem->lleno);

/* Cons. 2 */
for (;;) {
  sem_wait(&mem->lleno);  /* duerme sin gastar CPU */
  if (mem->fin) break;
  a = mem->a; b = mem->b;
  sem_post(&mem->vacio);
  res_sum[n++] = a + b;
}
```

### Cierre ordenado

```c
for (i = 0; i < 3; i++) wait(NULL);
sem_destroy(&mem->vacio);
sem_destroy(&mem->lleno);
shmdt(mem);
shmctl(shm_id, IPC_RMID, NULL);
unlink(FIFO_FILE);
```

Semáforos POSIX sin nombre dentro de `shm` (`pshared=1`), inicializados por el padre **antes** del `fork`: `vacio=1`, `lleno=0`. Compilar con `-pthread`.

> **Idea central:** el aislamiento de los procesos hace que comunicar cueste; compartir memoria es rápido pero **obliga** a sincronizar explícitamente.

---

## 3. IPC: mecanismos y criterios de elección

| Aspecto | FIFO (paso de mensajes) | Memoria compartida System V |
| :--- | :--- | :--- |
| Modelo | Flujo secuencial de bytes | Región accesible directamente, acceso aleatorio |
| Copias | Usuario → kernel → usuario | Ninguna tras `shmat` |
| Sincronización | Implícita: `read` bloquea si está vacío | Ninguna: debe agregarse |
| Límites de mensaje | No existen | Los define el programador |
| Lectores múltiples | Cada byte lo consume uno solo | Todos ven el mismo dato |
| Persistencia | Archivo especial hasta `unlink` | Kernel hasta `IPC_RMID` / `ipcrm` |

* **FIFO vacío:** `read` bloquea si hay escritores; devuelve `0` (EOF) si todos cerraron su extremo.
* **Más eficiente para datos:** memoria compartida (sin copias), con el costo de sincronizar. Para un flujo simple entre procesos, el FIFO es más seguro.
* **Liberar `shm` es obligatorio:** el segmento sobrevive al proceso (fuga de recurso del sistema). Se verifica con `ipcs -m` y `nattch`.
* **Otros mecanismos del curso:** pipes anónimos (parientes), señales (un evento, no se encolan), colas POSIX (mensajes con prioridad), sockets UNIX (bidireccional).

---

## 4. Sincronización: qué pasa cuando 2 o más procesos colaboran

### Conceptos
* **Condición de carrera:** el resultado depende del orden de intercalación. `contador++` son tres pasos (leer, sumar, escribir).
* **Sección crítica:** código que toca el recurso compartido.
* **Requisitos:** exclusión mutua, progreso, espera acotada.
* **Semáforo:** entero con `wait` (P) y `post` (V) atómicos. Binario = mutex; contador = unidades de recurso.
* **Productor-consumidor:** `vacio=N`, `lleno=0`, `mutex=1`; primero el semáforo de conteo, después el mutex.

> **Advertencia:** Los fallos son **no deterministas**: que no falle en pruebas no demuestra corrección (un solo núcleo puede ocultar la carrera que un multinúcleo muestra).

### Estrategias de mitigación
* **Reducir lo compartido:** `resultado_mul` y `resultado_sum` tienen un único escritor cada uno, sin conflicto.
* **Usar el canal como sincronizador:** el `read` bloqueante del FIFO.
* **Exclusión mutua:** semáforo/mutex.
* **Señalización por eventos:** `vacio`/`lleno`.

**Espera activa vs. bloqueante:** la primera mantiene la CPU al ~100 % con muchos cambios no voluntarios; la segunda duerme (≈0 % CPU, cambios voluntarios). Evidencia: `top`, `ps wchan`, `strace -f`, `ipcs`, `valgrind`.

**Acoplamiento:** con `SIGSTOP` al Consumidor 2, el 1 queda bloqueado en `sem_wait(vacio)`: el retraso de uno se transmite al otro.

---

## 5. Interbloqueos (deadlock)

### Condiciones de Coffman (las 4 a la vez)
1. **Exclusión mutua**
2. **Retener y esperar**
3. **Sin expropiación**
4. **Espera circular**

Basta romper una para prevenirlo. Con una instancia por recurso, **ciclo = deadlock**; con varias, ciclo es necesario pero no suficiente.

| Estrategia | Idea |
| :--- | :--- |
| Prevención | Romper una condición (orden global de recursos) |
| Evitación | Solo estados seguros (Banquero) |
| Detección | Buscar ciclos; abortar o expropiar |
| Ignorar | Algoritmo del avestruz |

### Deadlock en este laboratorio
* **Bloqueo por fin de datos (P8):** sin el campo `fin`, el Consumidor 2 espera para siempre un `sem_post(lleno)` que nunca llegará.
* **Semáforo retenido:** si se mata (`SIGKILL`) a un proceso que tenía el semáforo, los demás quedan bloqueados (en System V, `SEM_UNDO` lo mitiga).
* **Orden incorrecto:** en el buffer de N posiciones, tomar `mutex` antes que `vacio` con el buffer lleno bloquea a todos.

**Banquero:** estado *seguro* si existe una secuencia en que todos terminan. Ejemplo (12 unidades; P0 5/10, P1 2/4, P2 2/9; disponible 3): secuencia segura `<P1, P0, P2>`. Si P2 pide 1, solo P1 puede terminar y quedan P0 y P2 sin cupo: estado **inseguro**, se rechaza. Inseguro no significa deadlock, solo que podría ocurrir.

---

## 6. Planificación de procesos: niveles y algoritmos

### Tres niveles

| Nivel | Decide | Frecuencia | En el laboratorio |
| :--- | :--- | :--- | :--- |
| **Largo plazo** (admisión) | Qué programas se admiten como procesos; controla el grado de multiprogramación | Baja | Los tres `fork` del padre crean los procesos que pasan a "listo" |
| **Mediano plazo** (swapping) | Suspender o reactivar procesos en memoria secundaria según la carga de memoria | Media | No interviene con 3 procesos; se vería con memoria escasa |
| **Corto plazo** (despachador) | Qué proceso listo usa la CPU y por cuánto tiempo | Muy alta (ms) | Intercala Productor y Consumidores; `sem_wait`/`read` bloquean y `sem_post` los vuelve a "listo" |

> **Conexión clave:** el planificador de corto plazo es quien intercala las instrucciones y por eso aparecen las carreras. Un proceso bloqueado no consume CPU: el SO ejecuta a otro mientras tanto, ventaja de la espera bloqueante frente a la activa.

### Algoritmos de corto plazo y métricas

| Algoritmo | Criterio | Expropiativo | Ventaja | Desventaja |
| :--- | :--- | :--- | :--- | :--- |
| FCFS | Orden de llegada | No | Simple | Efecto convoy: los largos retrasan a los cortos |
| SPN / SJF | Menor ráfaga | No | Mínima espera media | Requiere conocer la duración; inanición de largos |
| SRTF | Menor tiempo restante | Sí | Excelente espera media | Inanición; sobrecarga de cambios |
| Round Robin | Quantum rotativo | Sí | Justo, buen tiempo de respuesta | Quantum pequeño = mucho cambio de contexto; grande = FCFS |
| Prioridades | Prioridad numérica | Ambos | Favorece lo crítico | Inanición (se corrige con *aging*) |
| Colas multinivel / CFS | Varias colas / menor `vruntime` | Sí | Adaptable; CFS reparte equitativamente (árbol rojo-negro) | Complejidad; ajuste con `nice` |

* **Tiempo de retorno** = fin − llegada. **Tiempo de espera** = retorno − ráfaga. **Tiempo de respuesta** = primer uso de CPU − llegada.
* **Procesos limitados por E/S** (Productor con `sleep`, Consumidores que bloquean) ceden la CPU pronto: los planificadores interactivos los favorecen. Los limitados por CPU agotan su quantum.
* **Prioridad y bloqueos:** `nice`/`renice` cambia la prioridad, pero no corrige una sincronización mal diseñada.

---

## Mapa de integración: del laboratorio a la teoría

| Pregunta de análisis | Concepto | Evidencia |
| :--- | :--- | :--- |
| Estados del ciclo de vida | Nuevo → listo → ejecución → bloqueado → terminado (zombi) | `ps -o pid,stat,wchan:20,cmd` |
| Mecanismo más eficiente | Memoria compartida (sin copias), con sincronización explícita | Comparación FIFO vs `shm` |
| Leer un FIFO vacío | `read` bloquea o devuelve 0 si no hay escritores | `pipe_read`, `read(...) = 0` en `strace` |
| Cambio de contexto | Voluntario (bloqueo) vs no voluntario (expulsión) | `/proc/<PID>/status`, `pidstat -w` |
| Liberar `shm` | Los segmentos persisten tras terminar el proceso | `ipcs -m` antes y después |
| `shm` vs FIFO temporales | Acceso directo sin sincronización vs flujo con sincronización implícita | Experimentos de competencia y sobrescritura |

---

## 7. Autoevaluación y banco de ideas para el examen

### Preguntas de comprensión

**1. ¿Qué información del proceso reside en el PCB y cuál en la Process Image?**
El PCB guarda lo que el SO necesita para gestionarlo (PID, estado, registros, prioridad, recursos abiertos). La imagen incluye código, datos, heap y pila en su espacio de direcciones, además del PCB.

**2. ¿Por qué el arreglo `malloc` de un consumidor no es visible para el otro?**
Tras `fork` cada proceso tiene su propio heap (copy-on-write); solo el segmento `shmat` es compartido.

**3. ¿Por qué dos lectores sobre un FIFO no reciben el mismo par?**
El FIFO entrega cada byte a un único lector; el kernel los reparte de forma no determinista.

**4. ¿Por qué una bandera `listo` no sustituye un semáforo?**
Verificar y modificar no es atómico, hay sobrescritura posible y, con optimización, el compilador puede dejar la variable en un registro.

**5. Un consumidor usa espera activa y otro `sem_wait`. ¿Qué difiere en estados, CPU y contexto?**
El primero alterna listo/ejecución (~100 % CPU, cambios no voluntarios); el segundo pasa a bloqueado (≈0 % CPU, cambios voluntarios).

**6. ¿Qué nivel de planificación decide quién usa la CPU y cuál controla la multiprogramación?**
Corto plazo decide la CPU; largo plazo controla cuántos procesos se admiten. El mediano plazo hace swapping.

**7. ¿Qué ocurre si falta el campo `fin`?**
El Consumidor 2 queda bloqueado para siempre en `sem_wait(lleno)`: un bloqueo por evento que nunca llega.

**8. ¿Qué condición de Coffman se rompe con un orden global de semáforos?**
La espera circular.

**9. ¿Por qué Round Robin favorece los procesos interactivos y SRTF puede causar inanición?**
RR da quantum a todos por turno sin conocer la duración; SRTF siempre prefiere al de menor tiempo restante y pospone indefinidamente a los largos.

**10. ¿Qué queda en el sistema si el padre muere con `kill -9`?**
El segmento `shm` (visible con `ipcs -m`) y el archivo FIFO; hay que limpiarlos con `ipcrm` y `unlink`. Los hijos huérfanos pasan a ser adoptados por init.

### Formatos de pregunta sugeridos
* **Código:** completar el consumidor con `sem_wait`/`sem_post` en el orden correcto; detectar el error en un fragmento (por ejemplo, `mutex` antes de `vacio`).
* **Análisis:** interpretar una salida de `ps`, `ipcs` o `strace` e identificar el problema (zombi, `nattch`, dos mensajes en un `read`).
* **Cálculo:** diagrama de Gantt con FCFS, SJF y RR, con tiempos de espera y retorno; seguridad de un estado con el Banquero.
* **Conceptual:** justificar el mecanismo IPC según parentesco, dirección y volumen; relacionar cada nivel de planificación con un hecho del laboratorio.

---
*Sistemas Operativos, Universidad Santiago de Cali, 2026B · Resumen del segundo corte*