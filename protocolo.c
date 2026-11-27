#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <unistd.h>

#include "protocolo.h"

int enviar_todo(int fd, const char *buf, size_t len)
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
