#include <string.h>
#include "monitor.h"

#define BD_FILAS 6

/* Tabla de la base de datos: static const, seis filas, en el orden del
   documento, con los títulos tal cual aparecen en la referencia. */
static const episodio_t bd[] = {
    { "S01E01", "The Hedge Knight", 9.2f, 45120u },
    { "S01E02", "Hard Salt Beef", 8.8f, 38900u },
    { "S01E03", "The Squire", 9.5f, 52400u },
    { "S01E04", "Seven", 9.1f, 41200u },
    { "S01E05", "In the Name of the Mother", 9.7f, 68300u },
    { "S01E06", "The Morrow", 9.4f, 49800u },
};

int bd_validar(void)
{
    size_t i;
    size_t cant = sizeof(bd) / sizeof(bd[0]);

    if ((int)cant != BD_FILAS) {
        return 0;
    }

    for (i = 0; i < cant; i++) {
        const episodio_t *e = &bd[i];

        if (e->codigo == NULL || e->titulo == NULL) {
            return 0;
        }
        if (strlen(e->codigo) != 6) {
            return 0;
        }
        if (e->puntuacion < 0.0f || e->puntuacion > 10.0f) {
            return 0;
        }
        if ((int)e->votos < 0) {
            return 0;
        }
    }

    return 1;
}

/* Busca un episodio por código exacto (sensible a mayúsculas).
   Devuelve NULL si el código es NULL, no mide 6 caracteres o no existe. */
const episodio_t *bd_buscar_interno(const char *codigo)
{
    size_t i;
    size_t cant = sizeof(bd) / sizeof(bd[0]);

    if (codigo == NULL) {
        return NULL;
    }
    if (strlen(codigo) != 6) {
        return NULL;
    }
    for (i = 0; i < cant; i++) {
        if (strcmp(bd[i].codigo, codigo) == 0) {
            return &bd[i];
        }
    }
    return NULL;
}
