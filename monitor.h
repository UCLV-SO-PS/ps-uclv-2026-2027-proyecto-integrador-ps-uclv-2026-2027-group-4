#ifndef MONITOR_H
#define MONITOR_H

#include <stddef.h>

typedef struct {
    const char *codigo;
    const char *titulo;
    float puntuacion;
    unsigned votos;
} episodio_t;

const episodio_t *bd_buscar(const char *codigo);
int bd_validar(void);
int formato_respuesta(const episodio_t *e, char *buf, size_t n);
const char *formato_error(void);

#endif
