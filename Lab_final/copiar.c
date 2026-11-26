// Cristian Camilo Cruz Gallego
// Sebastian Orejuela Mina
// Bono E2 - Copia de archivos con llamadas al sistema (SisOper 2026B)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Uso: %s <origen> <destino> <tam_buffer> [fsync]\n", argv[0]);
        return 1;
    }

    size_t tam_buf = (size_t)atol(argv[3]);
    if (tam_buf == 0) {
        fprintf(stderr, "Error: el tamaño del búfer debe ser mayor a 0.\n");
        return 1;
    }

    int usar_fsync = (argc == 5 && strcmp(argv[4], "fsync") == 0);

    char *buf = malloc(tam_buf);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    int fd_in = open(argv[1], O_RDONLY);
    if (fd_in == -1) {
        perror("open origen");
        free(buf);
        return 1;
    }

    int fd_out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        perror("open destino");
        close(fd_in);
        free(buf);
        return 1;
    }

    ssize_t leidos;
    while ((leidos = read(fd_in, buf, tam_buf)) > 0) {
        ssize_t escritos_total = 0;
        while (escritos_total < leidos) {
            ssize_t w = write(fd_out, buf + escritos_total, (size_t)(leidos - escritos_total));
            if (w == -1) {
                perror("write");
                close(fd_in);
                close(fd_out);
                free(buf);
                return 1;
            }
            escritos_total += w;
        }
    }

    if (leidos == -1) {
        perror("read");
        close(fd_in);
        close(fd_out);
        free(buf);
        return 1;
    }

    if (usar_fsync) {
        if (fsync(fd_out) == -1) {
            perror("fsync");
            close(fd_in);
            close(fd_out);
            free(buf);
            return 1;
        }
    }

    close(fd_in);
    close(fd_out);
    free(buf);
    return 0;
}
