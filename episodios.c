#include "monitor.h"

extern const episodio_t *bd_buscar_interno(const char *codigo);

const episodio_t *bd_buscar(const char *codigo)
{
    return bd_buscar_interno(codigo);
}
