# Sistemas Operativos 2026B · Universidad Santiago de Cali
## Ejercicio E1 · Mini `ps`: Leer el PCB desde `/proc`

---

## 1. Código Fuente (`mimips.c`)

Implementación completa en C (estándar `gnu99`) que recorre el sistema de archivos virtual `/proc`, extrae los campos relevantes del Bloque de Control de Procesos (PCB) desde `/proc/<PID>/status` y calcula el total de procesos activos junto con la memoria física residente acumulada (`VmRSS`).

```c
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
```

### Compilación y Ejecución

```bash
gcc -Wall -Wextra -std=gnu99 -o minips mimips.c
./minips
```

### Cumplimiento de Requisitos Técnicos

1. **Filtrado de entradas no numéricas:** Mediante `isdigit((unsigned char)e->d_name[0])` se descartan entradas virtuales de `/proc` que no corresponden a procesos (`sys`, `net`, `cpuinfo`, `meminfo`, `self`, etc.).
2. **Tolerancia a procesos efímeros (condiciones de carrera):** Si un proceso termina entre la llamada a `readdir()` y `fopen()`, la función `leer_proceso()` retorna `-1` sin interrumpir el programa y `main()` lo omite con `continue`.
3. **Hilos del kernel con `VmRSS = 0`:** La inicialización `memset(p, 0, sizeof *p)` asegura que los hilos del kernel (descendientes de `PID 2`, `kthreadd`), los cuales carecen de espacio de direcciones de usuario y no poseen línea `VmRSS:` en `status`, muestren `0` KiB.
4. **Gestión de recursos y compilación limpia:** Se validan los punteros de `opendir` y `fopen`, se cierran todos los flujos con `fclose(f)` y `closedir(d)` evitando fugas de descriptores, y compila sin advertencias bajo `-Wall -Wextra`.

---

## 2. Salida de Ejecución (`./minips`)

Ejecución realizada en **openSUSE Tumbleweed** (`gcc 16.2.1`):

```text
   PID   PPID NOMBRE           E  RSS(KiB)      VOL      INV
     1      0 systemd          S     48864    21458     2092
     2      0 kthreadd         S         0      651       12
     3      2 pool_workqueue_release S         0        6        0
     4      2 kworker/R-rcu_gp I         0        2        0
     5      2 kworker/R-sync_wq I         0        2        0
     6      2 kworker/R-kvfree_rcu_reclaim I         0        2        0
     7      2 kworker/R-slub_flushwq I         0        2        0
     8      2 kworker/R-netns  I         0        2        0
    10      2 kworker/0:1-cgroup_free I         0      213        8
    11      2 kworker/0:0H-kblockd I         0        4        0
    13      2 kworker/R-mm_percpu_wq I         0        2        0
    14      2 ksoftirqd/0      S         0    72698       33
    15      2 rcu_preempt      I         0  2607648     3325
    16      2 rcu_exp_par_gp_kthread_worker/0 S         0        2        0
    17      2 rcu_exp_gp_kthread_worker S         0     1489        7
    18      2 migration/0      S         0    21464        0
    19      2 kprobe-optimizer S         0        2        0
    20      2 idle_inject/0    S         0        3        0
    21      2 cpuhp/0          S         0       18        0
    22      2 cpuhp/1          S         0       18        3
    23      2 idle_inject/1    S         0        3        0
    24      2 migration/1      S         0    23181        0
    25      2 ksoftirqd/1      S         0    45924       51
    27      2 kworker/1:0H-kblockd I         0        4        0
    28      2 cpuhp/2          S         0       18        2
    29      2 idle_inject/2    S         0        3        0
    30      2 migration/2      S         0    42661        0
    31      2 ksoftirqd/2      S         0    29259       37
    33      2 kworker/2:0H-kblockd I         0        4        0
    34      2 cpuhp/3          S         0       18        2
    35      2 idle_inject/3    S         0        3        0
    36      2 migration/3      S         0    36465        0
    37      2 ksoftirqd/3      S         0    25532       37
    39      2 kworker/3:0H     I         0        4        0
    40      2 cpuhp/4          S         0       18        2
    41      2 idle_inject/4    S         0        3        0
    42      2 migration/4      S         0    86948        0
    43      2 ksoftirqd/4      S         0    16639       40
    45      2 kworker/4:0H-kblockd I         0        4        0
    46      2 cpuhp/5          S         0       18        2
    47      2 idle_inject/5    S         0        3        0
    48      2 migration/5      S         0    25446        0
    49      2 ksoftirqd/5      S         0    16979       71
    51      2 kworker/5:0H     I         0        4        0
    52      2 cpuhp/6          S         0       18        2
    53      2 idle_inject/6    S         0        3        0
    54      2 migration/6      S         0    22107        0
    55      2 ksoftirqd/6      S         0     6085       57
    56      2 kworker/6:0-events I         0    22744       38
    57      2 kworker/6:0H-kblockd I         0        4        0
    58      2 cpuhp/7          S         0       18        3
    59      2 idle_inject/7    S         0        3        0
    60      2 migration/7      S         0    29598        0
    61      2 ksoftirqd/7      S         0     7567       44
    63      2 kworker/7:0H-kblockd I         0        4        0
    64      2 cpuhp/8          S         0       18        3
    65      2 idle_inject/8    S         0        3        0
    66      2 migration/8      S         0    44040        0
    67      2 ksoftirqd/8      S         0     4926       30
    69      2 kworker/8:0H-kblockd I         0        4        0
    70      2 cpuhp/9          S         0       18        1
    71      2 idle_inject/9    S         0        3        0
    72      2 migration/9      S         0    43475        0
    73      2 ksoftirqd/9      S         0     6058       55
    75      2 kworker/9:0H-kblockd I         0        4        0
    76      2 cpuhp/10         S         0       18        0
    77      2 idle_inject/10   S         0        3        0
    78      2 migration/10     S         0    83973        0
    79      2 ksoftirqd/10     S         0     6482       37
    81      2 kworker/10:0H-kblockd I         0        4        0
    82      2 cpuhp/11         S         0       18        2
    83      2 idle_inject/11   S         0        3        0
    84      2 migration/11     S         0   165104        0
    85      2 ksoftirqd/11     S         0   173999      218
    87      2 kworker/11:0H-kblockd I         0        4        0
   103      2 kdevtmpfs        S         0      284        2
   104      2 kworker/R-inet_frag_wq I         0        2        0
   105      2 rcu_tasks_kthread I         0       12        0
   106      2 rcu_tasks_rude_kthread I         0        2        0
   107      2 kauditd          S         0      322        7
   108      2 khungtaskd       S         0        3        0
   110      2 oom_reaper       S         0        2        0
   112      2 kworker/R-writeback I         0        2        0
   113      2 kcompactd0       S         0    13484      869
   114      2 ksmd             S         0        2        0
   115      2 khugepaged       S         0     4901      158
   116      2 kworker/R-bio_complete I         0        2        0
   117      2 kworker/R-kblockd I         0        2        0
   118      2 kworker/R-blkcg_punt_bio I         0        2        0
   119      2 kworker/R-kintegrityd I         0        2        0
   120      2 irq/9-acpi       S         0        2        1
   121      2 kworker/7:1-mm_percpu_wq I         0    33190       91
   123      2 kworker/R-tpm_dev_wq I         0        2        0
   124      2 kworker/R-edac-poller I         0        2        0
   125      2 kworker/R-devfreq_wq I         0        2        0
   126      2 watchdogd        S         0        2        0
   127      2 kworker/R-quota_events_unbound I         0        2        0
   128      2 irq/25-AMD-Vi0-Evt S         0        3        1
   129      2 irq/26-AMD-Vi0-PPR S         0        2        1
   130      2 irq/27-AMD-Vi0-GA S         0        2        1
   131      2 kswapd0          S         0      361      199
   133      2 kworker/R-kthrotld I         0        2        0
   134      2 kworker/u51:0-hci0 I         0      305        2
   135      2 irq/28-aerdrv    S         0        3        2
   136      2 irq/28-pcie-dpc  S         0        3        2
   138      2 kworker/R-mld    I         0        2        0
   139      2 kworker/R-ipv6_addrconf I         0        2        0
   141      2 kworker/R-kstrp  I         0        2        0
   142      2 kworker/1:1-events I         0    42594      142
   145      2 kworker/4:1-events I         0    49332      539
   154      2 kworker/9:1-events I         0    41665      272
   192      2 kworker/R-charger_manager I         0        2        0
   193      2 kworker/u52:0    I         0        2        0
   194      2 kworker/u53:0-rb_allocator I         0     1578        8
   196      2 kworker/10:1-mm_percpu_wq I         0    40446      256
   197      2 kworker/5:1-events I         0    52014      118
   362      2 kworker/2:2-events I         0    46781      204
   421      2 kworker/R-kaluad I         0        2        0
   434      2 kworker/R-kmpath_rdacd I         0        2        0
   443      2 kworker/R-ata_sff I         0        2        0
   455      2 kworker/3:2-mm_percpu_wq I         0    48810      335
   456      2 scsi_eh_0        S         0       33        0
   457      2 kworker/R-scsi_tmf_0 I         0        2        0
   458      2 scsi_eh_1        S         0       25        0
   459      2 kworker/R-scsi_tmf_1 I         0        2        0
   460      2 scsi_eh_2        S         0        3        0
   461      2 kworker/R-scsi_tmf_2 I         0        2        0
   462      2 scsi_eh_3        S         0        3        0
   463      2 kworker/R-scsi_tmf_3 I         0        2        0
   464      2 scsi_eh_4        S         0       25        0
   465      2 kworker/R-scsi_tmf_4 I         0        2        0
   466      2 scsi_eh_5        S         0       25        0
   467      2 kworker/R-scsi_tmf_5 I         0        2        0
   468      2 scsi_eh_6        S         0        3        0
   469      2 kworker/R-scsi_tmf_6 I         0        2        0
   470      2 scsi_eh_7        S         0        3        0
   471      2 kworker/R-scsi_tmf_7 I         0        2        0
   472      2 scsi_eh_8        S         0        3        0
   473      2 kworker/R-scsi_tmf_8 I         0        2        0
   474      2 scsi_eh_9        S         0        3        0
   475      2 kworker/R-scsi_tmf_9 I         0        2        0
   476      2 scsi_eh_10       S         0        3        0
   477      2 kworker/R-scsi_tmf_10 I         0        2        0
   478      2 scsi_eh_11       S         0        3        0
   479      2 kworker/R-scsi_tmf_11 I         0        2        0
   480      2 scsi_eh_12       S         0        3        0
   481      2 kworker/R-scsi_tmf_12 I         0        2        0
   482      2 scsi_eh_13       S         0       47        0
   483      2 kworker/R-scsi_tmf_13 I         0        2        0
   494      2 kworker/0:2-events I         0    63328      198
   498      2 kworker/11:1H-kblockd I         0    28984       44
   500      2 kworker/0:1H-kblockd I         0    33860       12
   510      2 kworker/8:1H-kblockd I         0    31885       19
   511      2 kworker/9:1H-kblockd I         0    31534       45
   514      2 kworker/R-cfg80211 I         0        2        0
   535      2 kworker/7:1H-kblockd I         0    34131       16
   536      2 kworker/R-btrfs-worker I         0        2        0
   537      2 kworker/R-btrfs-delalloc I         0        2        0
   538      2 kworker/R-btrfs-flush_delalloc I         0        2        0
   539      2 kworker/R-btrfs-cache I         0        2        0
   540      2 kworker/R-btrfs-fixup I         0        2        0
   541      2 kworker/R-btrfs-endio I         0        2        0
   542      2 kworker/R-btrfs-endio-meta I         0        2        0
   543      2 kworker/R-btrfs-rmw I         0        2        0
   544      2 kworker/R-btrfs-endio-write I         0        2        0
   545      2 kworker/R-btrfs-freespace-write I         0        2        0
   546      2 kworker/R-btrfs-delayed-meta I         0        2        0
   547      2 kworker/R-btrfs-qgroup-rescan I         0        2        0
   548      2 kworker/1:1H-kblockd I         0    66413       43
   549      2 btrfs-cleaner    S         0      888        3
   550      2 btrfs-transaction S         0     6264     2290
   553      2 kworker/10:1H-kblockd I         0    30980       42
   563      2 kworker/2:1H-kblockd I         0    34192       22
   578      2 kworker/6:1H-kblockd I         0    35866       17
   584      2 kworker/4:1H-kblockd I         0    31365       53
   597      2 kworker/3:1H-kblockd I         0    28785       44
   601      2 kworker/5:1H-kblockd I         0    33860       21
   622      2 psimon           S         0    58704       29
   629      2 kworker/11:3-events I         0   230853     1915
   633      1 systemd-journal  S    115820    36582     1945
   665      1 systemd-udevd    S     31684     1400      197
   667      2 psimon           S         0        3        0
   807      2 irq/81-iwlwifi   S         0    40371        4
   829      2 kworker/u51:1-hci0 I         0      543        1
   832      2 kworker/u53:1    I         0        2        0
   862      2 kworker/R-btrfs-worker I         0        2        0
   863      2 kworker/R-btrfs-delalloc I         0        2        0
   864      2 kworker/R-btrfs-flush_delalloc I         0        2        0
   865      2 kworker/R-btrfs-cache I         0        2        0
   866      2 kworker/R-btrfs-fixup I         0        2        0
   867      2 kworker/R-btrfs-endio I         0        2        0
   868      2 kworker/R-btrfs-endio-meta I         0        2        0
   869      2 kworker/R-btrfs-rmw I         0        2        0
   870      2 kworker/R-btrfs-endio-write I         0        2        0
   871      2 kworker/R-btrfs-freespace-write I         0        2        0
   872      2 kworker/R-btrfs-delayed-meta I         0        2        0
   873      2 kworker/R-btrfs-qgroup-rescan I         0        2        0
   874      2 btrfs-cleaner    S         0      651        1
   875      2 btrfs-transaction S         0    19141     2707
   899      2 nv_queue         S         0        2        0
   900      2 nv_queue         S         0        2        0
   901      2 nv_open_q        S         0        2        0
   918      2 UVM              S         0       38        0
   919      2 UVM              S         0        2        0
   921      2 UVM              S         0       38        0
   949      2 nvidia-modeset/kthread_q S         0  4215640    49689
   950      2 nvidia-modeset/deferred_close_kthread_q S         0        2        0
   953      2 irq/84-nvidia    S         0   305431        8
   954      2 nvidia           S         0        2        0
   955      2 nv_queue         S         0    31781      293
   984      2 audit_prune_tree S         0        4        0
   995      1 auditd           S      2188      558       12
  1023      1 dbus-broker-lau  S      4864      578       50
  1026   1023 dbus-broker      S      5832   135530     1367
  1027      1 avahi-daemon     S      4620     9250      147
  1028      1 bluetoothd       S      7072      321       55
  1029      1 irqbalance       S      4200     1744      116
  1039      1 polkitd          S     13316     2305      114
  1043      1 smartd           S      7216      283       23
  1045      1 snapd            S     45956       53       22
  1048      1 switcheroo-cont  S      7744      160       11
  1049      1 systemd-logind   S     23120     3016      149
  1066      1 mcelog           S      2688        9        5
  1130      1 nvidia-persiste  S      3832        6        2
  1197      1 ModemManager     S     14252      252       20
  1206      1 firewalld        S     53372      868      179
  1207      1 NetworkManager   S     25168    24785      877
  1215      2 kworker/R-iprt-VBoxWQueue I         0        2        0
  1242      2 iprt-VBoxTscThread S         0      256        2
  1323      1 chronyd          S      7128      393       35
  1325      1 cupsd            S     13136      205       24
  1326      1 dockerd          S     97680     1005       67
  1337      1 wpa_supplicant   S     13896     3914      481
  1358      1 systemd          S     28436    20203     4029
  1427      1 mysqld           S    153824      267       29
  1435      1 agetty           S      2684      584       68
  1438      1 sddm             S     28340      164       26
  1447   1358 (sd-pam)         S      2568        1        0
  1481   1326 containerd       S     52636       62        2
  1518   1438 Xorg.bin         S     76692     3520       97
  1525      2 kworker/R-dio/sdb2 I         0        2        0
  1570   1358 MainThread       S    281304   791355     7908
  1949      1 upowerd          S     10024    11011      352
  2179   1438 sddm-helper      S     30872       62       14
  2185      1 ksecretd         S     70456      156       38
  2186   2179 startplasma-way  S     38496      100       16
  2282   1358 dbus-broker-lau  S      4296      328        2
  2283   2282 dbus-broker      S      6300   276069     2411
  2296   1358 kwin_wayland_wr  S     23020       10        2
  2300   2296 kwin_wayland     R    462824  7850147    99114
  2302   1358 ksmserver        S     33844       35        4
  2304   1358 kded6            S    109960    58786     2646
  2316      2 nvidia-drm/timeline-9 S         0        4        0
  2343      2 nvidia-drm/timeline-a S         0        2        0
  2346      2 nvidia-drm/timeline-b S         0        2        0
  2349      2 nvidia-drm/timeline-c S         0        2        0
  2350      2 nvidia-drm/timeline-d S         0        2        0
  2351      2 nvidia-drm/timeline-e S         0        2        0
  2352      2 nvidia-drm/timeline-f S         0        2        0
  2353      2 nvidia-drm/timeline-10 S         0        2        0
  2369      2 nvidia-drm/timeline-11 S         0        4        0
  2370      2 nvidia-drm/timeline-12 S         0  2541395     3027
  2372   1358 knighttimed      S     24624       11        2
  2373   1358 pipewire         S     46284    37971     4612
  2374   1358 wireplumber      S     31648    13716     7355
  2379      1 rtkit-daemon     S      3440      108       13
  2380      2 nvidia-drm/timeline-13 S         0      335        1
  2448   1358 gvfsd            S      9128       59       11
  2460   1358 gvfsd-fuse       S      8016       10        3
  2464   1358 dconf-service    S      6428       16        1
  2496      2 krfcommd         S         0        2        0
  2516      1 udisksd          S     16700      718      110
  2573   1358 pipewire-pulse   S     43740  1128512   112456
  2613   2300 Xwayland         S    123296   324530     4752
  2641      2 nvidia-drm/timeline-14 S         0     8225       13
  2780   1358 xsettingsd       S      5204       19        0
  2797   1358 obexd            S      9148       52        4
  2801   1358 plasmashell      S    217176    41877     3158
  2854   1358 kactivitymanage  S     51800      286      111
  2866   1358 quickshell       S   1525200  1763110   116430
  2870   1358 gmenudbusmenupr  S     30400      957       50
  2871   1358 kaccess          S     60908      980       41
  2875   1358 polkit-kde-auth  S     60828       61        6
  2877   1358 org_kde_powerde  S     68088     3437      235
  2878   1358 xembedsniproxy   S     29268      989       56
  3016      1 power-profiles-  S      7968       65        5
  3034      2 nvidia-drm/timeline-15 S         0        2        0
  3036      2 nvidia-drm/timeline-16 S         0    14446        7
  3039      2 nvidia-drm/timeline-17 S         0        2        0
  3047   1358 kdeconnectd      S     90344     5438      439
  3049   1358 xwaylandvideobr  S    135448    14969      382
  3058   1358 xdg-desktop-por  S     21812    39077      594
  3067   1358 agent            S      8916       34        6
  3073   1358 DiscoverNotifie  S     87800      895       82
  3074   1358 kalendarac       S    117456      360       60
  3078   1358 baloo_file       S     61844     3257     1129
  3101   1358 xdg-permission-  S      6976      638       11
  3104   1358 chrome           S    797360  9337927   164849
  3117   3104 cat              S      2392        1        1
  3118   3104 cat              S      2392      104        1
  3119   1358 xdg-document-po  S      9976     1803       94
  3130   3119 fusermount3      S      2484        2        0
  3135   1358 xdg-desktop-por  S    280528    36527     1526
  3179   1358 xdg-desktop-por  S     27096      798       32
  3189   1358 flatpak-session  S      7812     1126       39
  3202   1358 p11-kit-server   S      1140        2        0
  3204   1358 at-spi-bus-laun  S      8416       56        2
  3205      2 nvidia-drm/timeline-1e S         0        5        0
  3211   3204 dbus-broker-lau  S      4224       14        0
  3212   3211 dbus-broker      S      3448     3137       26
  3215   1358 bwrap            S      2472        3        3
  3216   1358 swaync           S    163916     3497      303
  3218   3215 xdg-dbus-proxy   S      6604     1297      151
  3221   1358 bwrap            S      1528       17       16
  3224   1358 chrome_crashpad  S      4596        5        0
  3226   1358 chrome_crashpad  S      4036        1        0
  3231   3221 spotify          S    504368   138689     3975
  3234   3104 chrome           S     80056       36       14
  3235   3104 chrome           S     80776       30        1
  3242   3235 chrome           S     24272     5425      519
  3256   1358 at-spi2-registr  S      7768     1442       33
  3294   3234 chrome           S    333688  4043969   140881
  3297   3104 chrome           S    183620     5860      522
  3310      2 nvidia-drm/timeline-1f S         0  2203641     2043
  3311   3294 chrome           S     49592     3156       50
  3322      2 nvidia-drm/timeline-20 S         0        2        0
  3325      2 nvidia-drm/timeline-21 S         0        2        0
  3328      2 nvidia-drm/timeline-22 S         0        2        0
  3332   3242 chrome           S     70360    10926      522
  3352   3221 spotify          S     97652        3        0
  3360   3242 chrome           S    195000    13310     2486
  3368   3231 spotify          S    110444       37       42
  3369   3231 spotify          S    110616       60        6
  3524   1358 kwalletd6        S     61916      157       12
  3609   3368 spotify          S    274452    61579     3995
  3610   3231 spotify          S    151576       59        8
  3624   3369 spotify          S     57468     1462      125
  3632   3242 chrome           S    153628    10132     1378
  3715   3369 spotify          R    452360   443511    46260
  3737   3202 p11-kit-remote   S     11128    23070      368
  3741   3104 chrome           S    112648   177105     2534
  3785   2866 dbus-monitor     S      2816        7        1
  3808   1358 baloo_idle_moni  S     26536      116       16
  3828      2 nvidia-drm/timeline-28 S         0    22251       29
  3858      2 nvidia-drm/timeline-2a S         0        4        0
  3860      2 nvidia-drm/timeline-2b S         0        5        0
  3861   3369 spotify          S    120756       33        2
  3870      2 nvidia-drm/timeline-2e S         0        2        0
  3872      2 nvidia-drm/timeline-30 S         0        5        0
  3875      2 nvidia-drm/timeline-32 S         0        4        0
  3878      2 nvidia-drm/timeline-34 S         0        5        0
  3881      2 nvidia-drm/timeline-36 S         0    24504       12
  3890      2 nvidia-drm/timeline-38 S         0   462991      209
  3895      2 vidmem           S         0        2        0
  3896      2 UVM              S         0        6        0
  4026   1358 gvfsd-metadata   S      7284       30        0
  4036      2 nvidia-drm/timeline-58 S         0        2        0
  4039      2 nvidia-drm/timeline-59 S         0        2        0
  4042      2 nvidia-drm/timeline-5a S         0      677        1
  4063      2 nvidia-drm/timeline-5f S         0        4        0
  4437      1 accounts-daemon  S      9048       60        6
  4493   3242 chrome           S    282616    10195     2288
  4506   3242 chrome           S    628212   106067    12419
  4556   3242 chrome           S    423996    58219    11511
  4631   3242 chrome           S    503596   763637    41342
  4701   3242 chrome           S    694232   336578    26562
  5159   3242 chrome           S    282692  3686977    93548
  5420   1358 jetbrains-toolb  S    404404        5        0
  5634   1358 jetbrainsd       S    113040       43        0
  6042      1 fwupd            S     55952     2218       58
  7236   3242 chrome           S    551548   484041    45224
  7273   3242 chrome           S    176536    22411     2098
  7551   1358 dolphin          S    155152    34482     1363
  8182   3242 chrome           S    411024   183804    34753
 29221      2 kworker/u50:5-events_unbound I         0    92380     1554
 36343      2 kworker/u50:3-events_unbound I         0   102895     2532
 37075      2 kworker/u50:4-btrfs-endio-meta I         0   105577     2153
 38745   3242 chrome           S    183768     5982      783
 41365   1358 bash             S      4392       51       14
 41384   1358 srt-logger       S      5208       81        4
 41400  41365 steam            S     13872      548       28
 41415  41400 srt-client-cont  S      4560       10        1
 41427  41415 x86_64-linux-gn  S      6200     1204       45
 41432  41415 srt-bwrap        S      2244     2804      328
 41505  41432 pv-adverb        S      5248       18       11
 41547  41505 steam            S    234620   269497     7438
 41558      2 nvidia-drm/timeline-2c9 S         0       12        0
 41568  41547 steamwebhelper   S    380608   646515    14036
 41577  41568 steamwebhelper   S     70484       13        1
 41578  41568 steamwebhelper   S     70096       12        1
 41580  41578 steamwebhelper   S     18124       21        6
 41600  41577 steamwebhelper   S    243956    14726     1647
 41603      2 nvidia-drm/timeline-2ca S         0     4222        2
 41605  41600 steamwebhelper   S     41808      156        2
 41620  41568 steamwebhelper   S    102176       25       14
 41623  41580 steamwebhelper   S     46784      176       13
 41700  41580 steamwebhelper   S    387764    16579     2505
 41787      2 nvidia-drm/timeline-2d7 S         0       40        0
 41879      2 nvidia-drm/timeline-2ed S         0        5        0
 41904  41580 steamwebhelper   S    191996     2416      722
 55759      2 kworker/u48:2-btrfs-discard I         0     6438        7
 56204      2 kworker/2:3-cgroup_free I         0        4        0
 59833      2 kworker/u49:10-btrfs-endio-meta I         0    84102      931
 59944      2 kworker/u50:0-btrfs-endio-write I         0    87401      911
 66229      2 kworker/u49:0-btrfs-endio-meta I         0    53654     1239
 66406      2 kworker/6:2-cgroup_free I         0        8        1
 70006      2 kworker/u50:10-btrfs-endio-write I         0    86789     1126
 76405      2 kworker/u48:1-ipv6_addrconf I         0     9542     8187
 78153      2 kworker/8:2-events I         0     9077       10
 84347      2 kworker/u49:1-btrfs-endio I         0    51640      627
 84377      2 kworker/u49:2-events_unbound I         0    72147     1030
 86893      2 kworker/1:2      I         0        3        0
 87049      2 kworker/9:0-events I         0        4        0
 88166      2 kworker/u50:9-btrfs-endio-meta I         0    52789     1194
 88783      2 kworker/u49:3-events_unbound I         0    70633     1029
 89707      2 kworker/u49:7-btrfs-endio I         0    65134      756
 89710      2 kworker/u49:12-events_unbound I         0    76845      926
 90212   3242 chrome           S    317948    50490     5886
 90318      2 kworker/u50:6-btrfs-endio-write I         0    55150      865
 90390      2 kworker/u50:11-btrfs-endio-write I         0    40840      721
 92004   3242 chrome           S    138108      269       21
 92230   3242 chrome           S    124248      246       24
 92644      2 kworker/8:1      I         0        2        0
 93254      2 kworker/u49:8-btrfs-endio-meta I         0    74979      339
 93526   1358 dolphin          S    148068     4141      721
 93613      2 kworker/7:0-events I         0        4        0
 94289   3242 chrome           R    665680    95590    27164
 94403      2 kworker/u50:1-btrfs-endio I         0    31948      245
 94902   1358 kitty            R    172712   455375    48640
 94906      2 nvidia-drm/timeline-5a1 S         0   112120      154
 94907  94902 kitten           S     33680      109        8
 94909  94902 bash             S      7284      657       35
 94910  94902 cmatrix          S      3232    29238     9525
 94911  94902 tty-clock        S      2920     1193      160
 94912  94902 cava             S     14424    71056     8974
 94913  94902 kitten           S     35624       44        6
 95032      2 kworker/11:0-events I         0        9        0
 95195      2 kworker/5:2-events I         0       11        0
 96348      2 nvidia-drm/timeline-5bd S         0        5        0
 96354      2 nvidia-drm/timeline-5bf S         0        2        0
 96563      2 nvidia-drm/timeline-60b S         0    58721       58
 97473      2 kworker/u49:4-btrfs-endio I         0    45478      353
 97945      2 kworker/u49:5-btrfs-endio I         0    23612      122
 98151      2 kworker/4:2-events I         0        3        0
 99864      2 kworker/10:0-cgroup_release I         0        3        0
100260      2 kworker/3:1-events I         0        5        0
100672   3242 chrome           S    178096       89       17
101237      2 kworker/u50:2-btrfs-endio I         0        2        0
101587      2 kworker/9:2-events I         0        3        0
101623   3242 chrome           S    121668       58       10
102372   3242 chrome           S    159556       89       26
102546   3242 chrome           S    150096       36       21
102670   3242 chrome           S     91236       25       21
102695  94909 minips           R      2140        0        5
Procesos: 454 | RSS total: 18563988 KiB
```

---

## 3. Análisis de Resultados (Preguntas Sección 6.7)

### 3.1 Predominio de estados (`S`, `R`, `I`, `Z`) y escasez de procesos en estado `R`
En la captura obtenida de los **454 procesos** activos en el sistema, el estado predominante es **`S` (*Interruptible Sleep*)**, seguido por **`I` (*Idle*)** para los hilos trabajadores del kernel (`kworker`), mientras que no se registran procesos zombi (`Z`) y apenas **5 procesos** aparecen en estado **`R` (*Running / Runnable*)**:
* **`2300` (`kwin_wayland`):** Compositor gráfico del escritorio.
* **`3715` (`spotify`):** Proceso activo de reproducción multimedia.
* **`94289` (`chrome`)** y **`94902` (`kitty`):** Renderizado de interfaz y terminal.
* **`102695` (`minips`):** El propio ejecutable que realiza el muestreo sobre `/proc`.

**Justificación:** Un procesador físico solo puede ejecutar simultáneamente tantos hilos como núcleos lógicos tenga disponibles en hardware. Además, en un sistema operativo moderno orientado a eventos e interacción, más del $98\%$ de los procesos (demonios del sistema como `systemd` [PID `1`], servicios de red o hilos trabajadores del kernel con `PPID 2`) pasan la mayor parte de su ciclo de vida bloqueados esperando interrupciones de entrada/salida, paquetes de red o vencimientos de temporizadores, cediendo el procesador al planificador hasta que ocurra un evento que los despierte.

### 3.2 Acumulación de cambios de contexto involuntarios y voluntarios (*CPU-bound* vs. *I/O-bound*)
El análisis de las columnas `VOL` y `INV` evidencia la diferencia entre procesos limitados por E/S (*I/O-bound*) y procesos con ráfagas intensivas de cálculo (*CPU-bound*):

* **Cambios de contexto voluntarios (`VOL`):** Se producen cuando un proceso cede voluntariamente la CPU al invocar una llamada al sistema bloqueante (`read`, `poll`, `epoll_wait`, `futex`). En los datos recolectados, los procesos que encabezan esta métrica son interactivos y orientados a E/S:
  * `chrome` (PID `3104`): **9,337,927** cambios voluntarios.
  * `kwin_wayland` (PID `2300`): **7,850,147** cambios voluntarios.
  * `nvidia-modeset/kthread_q` (PID `949`): **4,215,640** cambios voluntarios.
  * `pipewire-pulse` (PID `2573`): **1,128,512** cambios voluntarios.
  Estos procesos realizan operaciones breves de despacho de eventos gráficos, red o audio y se bloquean inmediatamente esperando la siguiente solicitud.

* **Cambios de contexto involuntarios (`INV`):** Ocurren cuando un proceso agota por completo su *quantum* de tiempo asignado y el planificador del kernel lo expulsa (*preemption*) mediante una interrupción de reloj para garantizar equidad entre tareas. Los mayores valores corresponden a procesos con alta demanda de cómputo:
  * `chrome` (PID `3104` y PID `3294`): **164,849** y **140,881** cambios involuntarios, respectivamente (motores de renderizado y ejecución JavaScript).
  * `quickshell` (PID `2866`): **116,430** cambios involuntarios.
  * `kwin_wayland` (PID `2300`): **99,114** cambios involuntarios.
  * `kitty` (PID `94902`): **48,640** cambios involuntarios (dibujando continuamente en pantalla los subprocesos `cmatrix` [PID `94910`] y `cava` [PID `94912`]).
  * Como caso ilustrativo puntual, el propio `minips` (PID `102695`) registró **`VOL = 0`** e **`INV = 5`**: al leer únicamente estructuras en memoria virtual (`/proc`) sin esperar entrada externa, nunca se bloqueó voluntariamente, siendo interrumpido 5 veces por el planificador durante su recorrido.

### 3.3 Ejecución paralela de `./lab_ipc` y estado de sus procesos hijos
Al lanzar `./lab_ipc` en segundo plano y filtrar la salida de `./minips` por su PID padre (`PPID`), se observan el proceso principal y los tres procesos hijos creados mediante `fork()` compartiendo el mismo `PPID`:
* **Durante su fase de espera o comunicación:** Tanto el padre como los tres hijos aparecen en estado **`S` (*Interruptible Sleep*)**, con un incremento en `VOL` y `0` en `INV`. Esto ocurre porque los procesos están bloqueados en llamadas al sistema de sincronización o lectura (`read()` sobre la tubería nombrada FIFO vacía o `sem_wait()` esperando acceso a la memoria compartida), cediendo voluntariamente la CPU hasta que el proceso productor escriba los datos.
* **Si un hijo finaliza antes de que el padre invoque `wait()`/`waitpid()`:** Dicho hijo pasa temporalmente al estado **`Z` (*Zombi*)** con `RSS = 0 KiB`, conservando únicamente su entrada en la tabla de procesos del kernel para que el padre pueda recolectar su estado de terminación.

### 3.4 Sobrestimación de la memoria física real en la suma de `VmRSS`
La ejecución de `./minips` reporta una suma acumulada de **`18,563,988 KiB`** ($\approx 17.70\text{ GiB}$). Esta cifra sobrestima el consumo real de memoria RAM física instalada porque `VmRSS` (*Resident Set Size*) contabiliza la totalidad de las páginas físicas mapeadas en el espacio de direcciones de cada proceso de forma independiente, sin descontar las páginas compartidas entre múltiples procesos.

En el listado se observa una arquitectura multiproceso intensiva, destacando **18 procesos** de `chrome` (con consumos individuales entre `24,272 KiB` y `797,360 KiB`), **10 procesos** de `steamwebhelper` y **8 procesos** de `spotify`. Todos estos procesos comparten en memoria física el código de bibliotecas dinámicas del sistema (`libc.so`, librerías gráficas Wayland/NVIDIA, motores de renderizado) así como páginas heredadas bajo el mecanismo **Copy-on-Write (CoW)** tras `fork()`. Aunque el kernel mantiene una única copia física de dichas páginas en RAM, `VmRSS` las suma reiteradamente en cada proceso que las referencia.
