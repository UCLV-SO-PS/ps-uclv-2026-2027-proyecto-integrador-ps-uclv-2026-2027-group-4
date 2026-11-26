#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "protocolo.h"

#define TAM_LINEA 64
#define TAM_LECTURA 256

static int conectar(void)
{
    int fd;
    struct sockaddr_in dir;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&dir, 0, sizeof dir);
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dir.sin_port = htons(PUERTO);

    if (connect(fd, (struct sockaddr *)&dir, sizeof dir) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

/* Envia el codigo y copia a stdout todo lo que responda el servidor
   hasta EOF. Devuelve 0 si la conversacion termino bien, -1 en error. */
static int consultar(const char *codigo)
{
    int fd;
    char solicitud[LONGITUD_CODIGO + 2];
    char buf[TAM_LECTURA];
    int ultimo = '\n';

    fd = conectar();
    if (fd < 0) {
        return -1;
    }

    snprintf(solicitud, sizeof solicitud, "%s\n", codigo);
    if (enviar_todo(fd, solicitud, strlen(solicitud)) < 0) {
        perror("write");
        close(fd);
        return -1;
    }

    /* El servidor ya tiene el codigo completo: avisamos el fin de nuestra
       escritura para que no espere mas datos. */
    (void)shutdown(fd, SHUT_WR);

    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read");
            close(fd);
            return -1;
        }
        if (n == 0) {
            break;
        }
        if (fwrite(buf, 1, (size_t)n, stdout) != (size_t)n) {
            close(fd);
            return -1;
        }
        ultimo = (unsigned char)buf[n - 1];
    }

    close(fd);

    if (ultimo != '\n') {
        putchar('\n');
    }
    fflush(stdout);

    return 0;
}

static int codigo_valido(const char *codigo)
{
    if (codigo == NULL || strlen(codigo) != LONGITUD_CODIGO) {
        fprintf(stderr,
                "ERROR: el codigo debe tener exactamente %d caracteres\n",
                LONGITUD_CODIGO);
        return 0;
    }
    return 1;
}

static void quitar_fin_de_linea(char *linea)
{
    size_t len = strlen(linea);

    while (len > 0 && (linea[len - 1] == '\n' || linea[len - 1] == '\r')) {
        linea[--len] = '\0';
    }
}

int main(int argc, char **argv)
{
    if (argc > 2) {
        fprintf(stderr, "uso: %s [codigo]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        if (!codigo_valido(argv[1])) {
            return EXIT_FAILURE;
        }
        return consultar(argv[1]) == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    for (;;) {
        char linea[TAM_LINEA];

        printf("Episodio (%d caracteres, 'salir' para terminar): ",
               LONGITUD_CODIGO);
        fflush(stdout);

        if (fgets(linea, sizeof linea, stdin) == NULL) {
            putchar('\n');
            break;
        }

        quitar_fin_de_linea(linea);

        if (strcmp(linea, "salir") == 0) {
            break;
        }
        if (linea[0] == '\0') {
            continue;
        }
        if (!codigo_valido(linea)) {
            continue;
        }

        (void)consultar(linea);
    }

    return EXIT_SUCCESS;
}
