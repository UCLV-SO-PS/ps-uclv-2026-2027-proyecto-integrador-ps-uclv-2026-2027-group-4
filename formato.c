#include <stdio.h>
#include "monitor.h"

static void formatear_votos(unsigned votos, char *buf, size_t n)
{
    char dig[16];
    size_t nd;
    size_t i;
    size_t j;

    if (n == 0) {
        return;
    }
    nd = (size_t)snprintf(dig, sizeof dig, "%u", votos);
    if (nd == 0 || nd >= sizeof dig) {
        buf[0] = '\0';
        return;
    }

    j = 0;
    for (i = 0; i < nd && j + 1 < n; i++) {
        if (i > 0 && (nd - i) % 3 == 0) {
            buf[j++] = ',';
        }
        buf[j++] = dig[i];
    }
    buf[j] = '\0';
}

int formato_respuesta(const episodio_t *e, char *buf, size_t n)
{
    char votos[16];
    int rc;

    if (e == NULL || buf == NULL || n == 0) {
        return -1;
    }

    formatear_votos(e->votos, votos, sizeof votos);

    rc = snprintf(buf, n,
                  "Codigo: %s\n"
                  "Titulo: %s\n"
                  "Puntuacion: %.1f\n"
                  "Votos: %s",
                  e->codigo, e->titulo, (double)e->puntuacion, votos);
    if (rc < 0 || (size_t)rc >= n) {
        return -1;
    }
    return rc;
}

const char *formato_error(void)
{
    return "ERROR: episodio no encontrado";
}
