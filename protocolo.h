#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>

#define PUERTO 6666
#define LONGITUD_CODIGO 6

int enviar_todo(int fd, const char *buf, size_t len);

#endif
