# Servidor Concurrente TCP de Calificaciones IMDb

Proyecto final de Programación de Sistemas. Servidor TCP en C que atiende
consultas de episodios de *A Knight of the Seven Kingdoms* en el puerto 6666,
con un proceso hijo por conexión.

## Integrantes

| Integrante | Parte del trabajo | Nota |
| --- | --- | --- |
| Machado | Biblioteca `libmonitor.a` | 5 |
| Felix | Protocolo cliente/servidor | 5 |
| Hector Yanes | Concurrencia, señales y verificación | 5 |

## Compilación

Requiere Linux, `gcc` con C11, `make` y `ar`.

```bash
make clean && make all
```

Genera `libmonitor.a`, `server` y `client` en la raíz. Los objetos intermedios
van a `build/`.

```bash
make check              # prueba de la biblioteca
scripts/acceptance.sh   # verificación completa del sistema
make clean              # borra todo lo generado
```

## Ejecución

```bash
./server            # escucha en 0.0.0.0:6666
./client S01E01     # consulta un episodio
./client            # modo interactivo
```

El servidor no imprime nada al arrancar. Por cada conexión el hijo le manda al
padre la IP del cliente por una tubería, y el padre imprime:

```text
IP 127.0.0.1 PID 4213
```

`Ctrl+C` no lo detiene porque `SIGINT` está ignorada. Para terminarlo:
`kill -TERM <pid>`.

## Protocolo

El cliente envía el código de 6 caracteres seguido de `\n`. El servidor tolera
`\r\n` y rechaza cualquier otra longitud. La respuesta siempre termina en `\n`.

```text
Codigo: S01E01
Titulo: The Hedge Knight
Puntuacion: 9.2
Votos: 45,120
```

```text
ERROR: episodio no encontrado
```

## Prueba automática

```bash
./server --test-run; echo $?
```

Valida la base de datos, levanta el socket, se conecta a sí mismo y comprueba la
respuesta de `S01E01`. Devuelve 0 si todo funciona y distinto de 0 si el puerto
está ocupado o la base no es válida.

## Base de datos

| Código | Título | Puntuación | Votos |
| --- | --- | --- | --- |
| S01E01 | The Hedge Knight | 9.2 | 45,120 |
| S01E02 | Hard Salt Beef | 8.8 | 38,900 |
| S01E03 | The Squire | 9.5 | 52,400 |
| S01E04 | Seven | 9.1 | 41,200 |
| S01E05 | In the Name of the Mother | 9.7 | 68,300 |
| S01E06 | The Morrow | 9.4 | 49,800 |

La tabla vive en `bd.c`, dentro de `libmonitor.a`. La búsqueda es exacta y
sensible a mayúsculas.

## Archivos

```text
Makefile        compilación
monitor.h       contrato de la biblioteca
bd.c            tabla y validación
episodios.c     búsqueda por código
formato.c       formato de respuesta y error
protocolo.h     puerto, longitud del código y envío completo
protocolo.c
server.c        servidor concurrente
client.c        cliente de prueba
tests/          prueba de la biblioteca
scripts/        verificación de aceptación
screenshot/     capturas
```

## Decisiones de diseño

- Un proceso hijo por conexión. El padre solo acepta y hace `fork()`; el hijo
  cierra el socket de escucha, atiende una conexión y termina con `_exit()`.
- Una sola tubería, creada antes del bucle. El padre mantiene abierto el extremo
  de escritura: si lo cerrara, el de lectura estaría siempre listo y `select()`
  giraría al 100 % de CPU.
- El aviso mide menos que `PIPE_BUF` y sale en un solo `write()`, así que dos
  hijos no lo intercalan.
- `select()` sobre el socket y la tubería, para no bloquear el `accept()` mientras
  se espera un aviso.
- `SIGCHLD` solo cosecha con `waitpid(-1, NULL, WNOHANG)` en un bucle. Nada de
  `printf` dentro del handler.
- `SIGPIPE` ignorada: si el cliente corta antes de leer, el hijo cierra en
  `EPIPE` en lugar de morir.
- `SO_REUSEADDR` para poder reiniciar sin esperar `TIME_WAIT`.
- La biblioteca formatea el texto; el servidor lo envía y cierra la última línea.
- `--test-run` no crea tubería ni hace `fork()`, porque no tiene hijos.

## Capturas

| Archivo | Contenido |
| --- | --- |
| `01-servidor-y-clientes.png` | Varias consultas concurrentes y las líneas IP/PID |
| `02-consulta-valida.png` | Respuesta completa de un episodio |
| `03-codigo-invalido.png` | Respuesta de error |
| `04-prueba-automatica.png` | `--test-run` con el puerto libre y ocupado |
| `05-aceptacion.png` | Corrida completa de la aceptación |
