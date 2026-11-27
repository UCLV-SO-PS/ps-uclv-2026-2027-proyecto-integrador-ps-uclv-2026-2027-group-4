#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "monitor.h"
#include "protocolo.h"

#define BACKLOG 16
#define TAM_CODIGO 32
#define TAM_BLOQUE 256

struct aviso {
    pid_t pid;
    char ip[INET_ADDRSTRLEN];
};

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

static void crear_tuberia(int piper[2])
{
    if (pipe(piper) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
}

static void cosechar_hijos(int signo)
{
    int guardado = errno;

    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }
    errno = guardado;
}

static void instalar_manejadores(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction(SIGINT)");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGPIPE, &sa, NULL) < 0) {
        perror("sigaction(SIGPIPE)");
        exit(EXIT_FAILURE);
    }

    sa.sa_handler = cosechar_hijos;
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction(SIGCHLD)");
        exit(EXIT_FAILURE);
    }
}

static void avisar_padre(int fd, const struct sockaddr_in *cliente)
{
    struct aviso a;
    ssize_t escrito;

    memset(&a, 0, sizeof a);
    a.pid = getpid();

    if (inet_ntop(AF_INET, &cliente->sin_addr, a.ip, sizeof a.ip) == NULL) {
        perror("inet_ntop");
        return;
    }

    escrito = write(fd, &a, sizeof a);
    if (escrito < 0) {
        perror("write(aviso)");
        return;
    }
    if ((size_t)escrito != sizeof a) {
        fprintf(stderr, "aviso truncado: %zd de %zu bytes\n",
                escrito, sizeof a);
    }
}

static void leer_aviso(int fd)
{
    struct aviso a;
    ssize_t leido = read(fd, &a, sizeof a);

    if (leido < 0) {
        if (errno != EINTR) {
            perror("read(aviso)");
        }
        return;
    }
    if (leido == 0) {
        return;
    }
    if ((size_t)leido != sizeof a) {
        fprintf(stderr, "aviso truncado: %zd de %zu bytes\n",
                leido, sizeof a);
        return;
    }

    printf("IP %s PID %ld\n", a.ip, (long)a.pid);
    fflush(stdout);
}

static void responder(int fd, const char *texto)
{
    size_t largo = strlen(texto);

    if (largo > 0 && enviar_todo(fd, texto, largo) < 0) {
        return;
    }
    if (largo == 0 || texto[largo - 1] != '\n') {
        (void)enviar_todo(fd, "\n", 1);
    }
}

static void atender_cliente(int fd)
{
    char codigo[TAM_CODIGO];
    char bloque[TAM_BLOQUE];
    const episodio_t *episodio;

    if (read_code(fd, codigo, sizeof codigo) < 0) {
        responder(fd, formato_error());
        return;
    }

    episodio = bd_buscar(codigo);
    if (episodio == NULL) {
        responder(fd, formato_error());
        return;
    }

    if (formato_respuesta(episodio, bloque, sizeof bloque) < 0) {
        responder(fd, formato_error());
        return;
    }

    responder(fd, bloque);
}

static void aceptar_conexion(int escucha, int piper[2])
{
    struct sockaddr_in cliente;
    socklen_t tam = sizeof cliente;
    int conexion;
    pid_t pid;

    conexion = accept(escucha, (struct sockaddr *)&cliente, &tam);
    if (conexion < 0) {
        if (errno != EINTR) {
            perror("accept");
        }
        return;
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        close(conexion);
        return;
    }

    if (pid == 0) {
        close(escucha);
        close(piper[0]);
        avisar_padre(piper[1], &cliente);
        close(piper[1]);
        atender_cliente(conexion);
        shutdown(conexion, SHUT_WR);
        close(conexion);
        _exit(EXIT_SUCCESS);
    }

    close(conexion);
}

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

    if (enviar_todo(cliente, "S01E01\n", 7) < 0) {
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
    int piper[2];
    int maxfd;

    if (argc > 1) {
        if (strcmp(argv[1], "--test-run") == 0) {
            prueba_automatica();
        }
        fprintf(stderr, "uso: %s [--test-run]\n", argv[0]);
        return EXIT_FAILURE;
    }

    instalar_manejadores();
    crear_tuberia(piper);
    escucha = crear_escucha();
    maxfd = (escucha > piper[0]) ? escucha : piper[0];

    for (;;) {
        fd_set lectores;
        int listos;

        FD_ZERO(&lectores);
        FD_SET(escucha, &lectores);
        FD_SET(piper[0], &lectores);

        listos = select(maxfd + 1, &lectores, NULL, NULL, NULL);
        if (listos < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("select");
            continue;
        }

        if (FD_ISSET(piper[0], &lectores)) {
            leer_aviso(piper[0]);
        }

        if (FD_ISSET(escucha, &lectores)) {
            aceptar_conexion(escucha, piper);
        }
    }

    return EXIT_SUCCESS;
}
