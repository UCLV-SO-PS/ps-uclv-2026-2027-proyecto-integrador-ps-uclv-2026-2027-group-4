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

#include "monitor.h"

#define PUERTO 6666
#define BACKLOG 16
#define LONGITUD_CODIGO 6
#define TAM_CODIGO 32
#define TAM_BLOQUE 256

/* Escribe todo el buffer, reintentando en EINTR y cubriendo escrituras
   parciales. Devuelve 0 si escribio todo, -1 en error. */
static int write_all(int fd, const char *buf, size_t len)
{
    size_t enviado = 0;

    while (enviado < len) {
        ssize_t n = write(fd, buf + enviado, len - enviado);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        enviado += (size_t)n;
    }
    return 0;
}

/* Lee un codigo de episodio hasta '\n' o hasta llenar el buffer.
   Tolera terminaciones "\r\n" y exige exactamente seis caracteres.
   Devuelve 0 si el codigo es valido, -1 en caso contrario. */
static int read_code(int fd, char *codigo, size_t n)
{
    size_t len = 0;

    if (n < LONGITUD_CODIGO + 1) {
        return -1;
    }

    for (;;) {
        char c;
        ssize_t r = read(fd, &c, 1);

        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (r == 0 || c == '\n') {
            break;
        }
        if (len + 1 >= n) {
            return -1;
        }
        codigo[len++] = c;
    }

    if (len > 0 && codigo[len - 1] == '\r') {
        len--;
    }
    codigo[len] = '\0';

    if (len != LONGITUD_CODIGO) {
        return -1;
    }
    return 0;
}

/* Crea el socket de escucha en INADDR_ANY:6666. Cualquier fallo se
   informa con perror y termina el proceso. */
static int crear_escucha(void)
{
    int fd;
    int uno = 1;
    struct sockaddr_in dir;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &uno, sizeof uno) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(fd);
        exit(EXIT_FAILURE);
    }

    memset(&dir, 0, sizeof dir);
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = htonl(INADDR_ANY);
    dir.sin_port = htons(PUERTO);

    if (bind(fd, (struct sockaddr *)&dir, sizeof dir) < 0) {
        perror("bind");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (listen(fd, BACKLOG) < 0) {
        perror("listen");
        close(fd);
        exit(EXIT_FAILURE);
    }

    return fd;
}

/* Atiende una unica conexion: lee el codigo, busca el episodio y
   responde con el bloque formateado o con la linea de error. */
static void atender_cliente(int fd)
{
    char codigo[TAM_CODIGO];
    char bloque[TAM_BLOQUE];
    const episodio_t *episodio;
    const char *respuesta;
    size_t largo;

    if (read_code(fd, codigo, sizeof codigo) < 0) {
        respuesta = formato_error();
        (void)write_all(fd, respuesta, strlen(respuesta));
        return;
    }

    episodio = bd_buscar(codigo);
    if (episodio == NULL) {
        respuesta = formato_error();
        (void)write_all(fd, respuesta, strlen(respuesta));
        return;
    }

    if (formato_respuesta(episodio, bloque, sizeof bloque) < 0) {
        respuesta = formato_error();
        (void)write_all(fd, respuesta, strlen(respuesta));
        return;
    }

    largo = strlen(bloque);
    (void)write_all(fd, bloque, largo);
}

/* Modo --test-run: valida la base de datos, levanta el socket, se conecta
   a si mismo y comprueba la respuesta completa de S01E01 sin usar fork().
   Termina con 0 si todo funciona y con distinto de 0 si algo falla. */
static void prueba_automatica(void)
{
    int escucha;
    int cliente;
    int conexion;
    struct sockaddr_in dir;
    char respuesta[TAM_BLOQUE];
    size_t total = 0;

    if (!bd_validar()) {
        fprintf(stderr, "test-run: la base de datos de episodios no es valida\n");
        exit(EXIT_FAILURE);
    }

    escucha = crear_escucha();

    cliente = socket(AF_INET, SOCK_STREAM, 0);
    if (cliente < 0) {
        perror("test-run: socket cliente");
        close(escucha);
        exit(EXIT_FAILURE);
    }

    memset(&dir, 0, sizeof dir);
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dir.sin_port = htons(PUERTO);

    if (connect(cliente, (struct sockaddr *)&dir, sizeof dir) < 0) {
        perror("test-run: connect");
        close(cliente);
        close(escucha);
        exit(EXIT_FAILURE);
    }

    conexion = accept(escucha, NULL, NULL);
    if (conexion < 0) {
        perror("test-run: accept");
        close(cliente);
        close(escucha);
        exit(EXIT_FAILURE);
    }

    if (write_all(cliente, "S01E01\n", 7) < 0) {
        perror("test-run: write");
        close(conexion);
        close(cliente);
        close(escucha);
        exit(EXIT_FAILURE);
    }

    atender_cliente(conexion);
    close(conexion);

    for (;;) {
        ssize_t n;

        if (total >= sizeof respuesta - 1) {
            break;
        }
        n = read(cliente, respuesta + total, sizeof respuesta - 1 - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("test-run: read");
            close(cliente);
            close(escucha);
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;
    }
    respuesta[total] = '\0';

    close(cliente);
    close(escucha);

    if (strstr(respuesta, "Codigo: S01E01") == NULL ||
        strstr(respuesta, "Titulo: The Hedge Knight") == NULL ||
        strstr(respuesta, "Votos: 45,120") == NULL) {
        fprintf(stderr, "test-run: respuesta inesperada: %s\n", respuesta);
        exit(EXIT_FAILURE);
    }

    printf("test-run: OK\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    int escucha;

    if (argc > 1) {
        if (strcmp(argv[1], "--test-run") == 0) {
            prueba_automatica();
        }
        fprintf(stderr, "uso: %s [--test-run]\n", argv[0]);
        return EXIT_FAILURE;
    }

    escucha = crear_escucha();

    printf("Servidor escuchando en el puerto %d\n", PUERTO);
    fflush(stdout);

    for (;;) {
        int conexion;
        pid_t pid;

        conexion = accept(escucha, NULL, NULL);
        if (conexion < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            continue;
        }

        pid = fork();
        if (pid < 0) {
            perror("fork");
            close(conexion);
            continue;
        }

        if (pid == 0) {
            /* Hijo: no necesita el socket de escucha y atiende una sola
               conexion. _exit evita vaciar dos veces los buffers de stdio
               heredados del padre. */
            close(escucha);
            atender_cliente(conexion);
            shutdown(conexion, SHUT_WR);
            close(conexion);
            _exit(EXIT_SUCCESS);
        }

        /* Padre: cierra la conexion aceptada y vuelve a aceptar. */
        close(conexion);
    }

    return EXIT_SUCCESS;
}
