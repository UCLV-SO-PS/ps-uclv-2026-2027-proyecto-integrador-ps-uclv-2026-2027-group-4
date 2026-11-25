# Servidor Concurrente TCP — Calificaciones IMDb

Proyecto final de **Programación de Sistemas**. Servidor concurrente TCP en C
que atiende consultas de episodios de *A Knight of the Seven Kingdoms* en el
puerto **6666**, con arquitectura cliente-servidor y un proceso hijo por
conexión.

> Estado: esqueleto de G2. La documentación final, las capturas y los
> artefactos regenerados se completan en G4 (`feature/testing`).

## Integrantes

| Rol | Integrante | Slice |
| --- | --- | --- |
| Persona A | Machado | G1 — `libmonitor.a` |
| Persona B | Felix | G2 — protocolo cliente/servidor |
| Persona C | Hector Yanes | G3 — IPC y señales · G4 — testing |

## Requisitos

- Linux (se usan `fork`, `pipe`, `select`, sockets POSIX y señales).
- `gcc` con soporte C11.
- `make`.
- Opcional para pruebas manuales: `netcat` (`nc`).

## Compilación

```bash
make clean && make all
```

`make all` genera:

- `libmonitor.a` — base de datos de episodios y formateo de respuesta.
- `server` — servidor concurrente TCP.
- `client` — cliente/consola de prueba.

Otros objetivos:

```bash
make check   # compila y ejecuta tests/lib_check
make clean   # borra *.o, libmonitor.a, server, client y tests/lib_check
```

## Ejecución

Servidor:

```bash
./server
```

El servidor escucha en `0.0.0.0:6666`. Por cada conexión aceptada crea un
proceso hijo con `fork()` y el padre vuelve a esperar conexiones.

Cliente:

```bash
./client S01E01     # consulta un episodio
./client S99E99     # codigo inexistente: muestra el error del servidor
./client S01E0      # rechazo local: el codigo debe medir 6 caracteres
./client            # modo interactivo; se sale con 'salir' o Ctrl+D
```

Prueba directa del protocolo con `netcat`:

```bash
printf 'S01E05\n' | nc 127.0.0.1 6666
```

## Protocolo

1. El cliente se conecta a `127.0.0.1:6666` y envía el código de episodio de
   **exactamente 6 caracteres** seguido de `\n` (por ejemplo, `S01E01`).
2. El servidor acumula la solicitud hasta `\n`, tolera terminaciones `\r\n` y
   rechaza cualquier longitud distinta de 6.
3. El hijo busca el código con `bd_buscar()` y responde con el bloque que
   produce `formato_respuesta()` de `libmonitor.a`.
4. El servidor hace `shutdown(fd, SHUT_WR)` y cierra; el cliente lee hasta EOF.

### Formato de respuesta (contrato congelado)

Éxito, cuatro líneas:

```text
Codigo: S01E01
Titulo: The Hedge Knight
Puntuacion: 9.2
Votos: 45,120
```

Error, una línea:

```text
ERROR: episodio no encontrado
```

El bloque lo genera la biblioteca, no `server.c`. Este formato no cambia en los
siguientes slices: el script de aceptación de G4 compara contra él.

## Prueba automática (`--test-run`)

```bash
./server --test-run
echo "exit=$?"
```

El modo de prueba valida la base de datos, crea el socket en el puerto 6666, se
conecta a sí mismo, comprueba la respuesta completa de `S01E01` y termina con
código `0`. Si la base de datos no es válida o el puerto está ocupado, informa
por `stderr` y termina con código distinto de `0`.

## Base de datos

| Código | Título | Puntuación | Votos |
| --- | --- | --- | --- |
| S01E01 | The Hedge Knight | 9.2 | 45,120 |
| S01E02 | Hard Salt Beef | 8.8 | 38,900 |
| S01E03 | The Squire | 9.5 | 52,400 |
| S01E04 | Seven | 9.1 | 41,200 |
| S01E05 | In the Name of the Mother | 9.7 | 68,300 |
| S01E06 | The Morrow | 9.4 | 49,800 |

La tabla vive como `static const` en `bd.c`, dentro de `libmonitor.a`. La
búsqueda es exacta y sensible a mayúsculas.

## Estructura del repositorio

```text
Makefile        reglas de compilación
monitor.h       contrato público de la biblioteca
bd.c            tabla de episodios y validación
episodios.c     búsqueda por código
formato.c       formateo de respuesta y error
libmonitor.a    biblioteca estática (generada)
server.c        servidor concurrente TCP
client.c        cliente/consola de prueba
tests/          comprobaciones de la biblioteca
```

## Decisiones de diseño

- **Un hijo por conexión.** El padre solo acepta y hace `fork()`; el hijo cierra
  `listenfd`, atiende una única conexión y termina con `_exit()` para no vaciar
  dos veces los buffers de `stdio` heredados.
- **`SO_REUSEADDR`.** Permite reiniciar el servidor sin esperar `TIME_WAIT`.
- **Lectura y escritura completas.** `read_code()` y `write_all()` cubren
  lecturas/escrituras parciales y `EINTR`; no se asume que un `read()` o
  `write()` entrega todo el bloque.
- **Formato centralizado.** El servidor nunca arma el texto de respuesta: lo
  produce `libmonitor.a`.
- **`--test-run` sin `fork()`.** La prueba se conecta al propio socket y atiende
  la conexión de forma síncrona, reutilizando el mismo camino que un cliente
  real.

## Limitaciones conocidas de este slice

Estas limitaciones son deliberadas y se resuelven en G3 (`feature/concurrencia`):

- No se captura `SIGCHLD`: los hijos terminados quedan como `defunct` hasta que
  el padre termina.
- No se ignora `SIGINT`: `Ctrl+C` detiene el servidor.
- No se ignora `SIGPIPE`: si un cliente corta antes de leer, el hijo puede
  recibir la señal al escribir.
- No hay tubería: el padre todavía no imprime la IP ni el PID de cada cliente.
