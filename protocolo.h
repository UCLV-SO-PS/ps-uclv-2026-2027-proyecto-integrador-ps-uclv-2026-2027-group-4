#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <stddef.h>

/* Puerto TCP donde escucha el servidor y al que se conecta el cliente. */
#define PUERTO 6666

/* El codigo de episodio que viaja por el socket mide siempre 6 caracteres.
   Servidor y cliente tienen que estar de acuerdo, por eso vive aca. */
#define LONGITUD_CODIGO 6

/* Escribe len bytes de buf en fd, cubriendo escrituras parciales y
   reintentando en EINTR. Devuelve 0 si escribio todo, -1 en error. */
int enviar_todo(int fd, const char *buf, size_t len);

#endif /* PROTOCOLO_H */
