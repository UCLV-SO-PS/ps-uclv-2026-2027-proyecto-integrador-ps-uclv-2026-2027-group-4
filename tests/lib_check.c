#include <stdio.h>
#include <string.h>
#include "monitor.h"

static int fallos = 0;

static void verificar(int condicion, const char *mensaje)
{
    if (!condicion) {
        fallos++;
        printf("FALLO: %s\n", mensaje);
    }
}

int main(void)
{
    const episodio_t *e;
    char buf[256];
    int rc;
    float puntuacion_esperada = 9.7f;

    verificar(bd_validar() != 0, "bd_validar devuelve verdad");

    e = bd_buscar("S01E05");
    verificar(e != NULL, "S01E05 encontrado");
    if (e != NULL) {
        verificar(strcmp(e->codigo, "S01E05") == 0,
                  "S01E05: codigo correcto");
        verificar(strcmp(e->titulo, "In the Name of the Mother") == 0,
                  "S01E05: titulo correcto");
        verificar(e->puntuacion == puntuacion_esperada,
                  "S01E05: puntuacion correcta");
        verificar(e->votos == 68300u, "S01E05: votos correctos");
    }

    verificar(bd_buscar("S99E99") == NULL, "S99E99 devuelve NULL");
    verificar(bd_buscar("S01E0") == NULL, "S01E0 devuelve NULL");
    verificar(bd_buscar("") == NULL, "codigo vacio devuelve NULL");
    verificar(bd_buscar(NULL) == NULL, "NULL devuelve NULL");
    verificar(bd_buscar("S01E01X") == NULL, "S01E01X no es S01E01");
    verificar(bd_buscar("s01e05") == NULL,
              "la busqueda es sensible a mayusculas");

    e = bd_buscar("S01E01");
    verificar(e != NULL, "S01E01 encontrado");
    rc = formato_respuesta(e, buf, sizeof buf);
    verificar(rc > 0 && strstr(buf, "The Hedge Knight") != NULL,
              "el bloque de exito tiene el titulo");
    verificar(rc > 0 && strstr(buf, "45,120") != NULL,
              "el bloque de exito tiene los votos con coma");

    verificar(strcmp(formato_error(), "ERROR: episodio no encontrado") == 0,
              "formato_error devuelve la linea de error");

    if (fallos == 0) {
        printf("lib_check: todo OK\n");
    }
    return fallos == 0 ? 0 : 1;
}
