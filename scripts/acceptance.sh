#!/usr/bin/env bash
#
# scripts/acceptance.sh — verificación de aceptación del proyecto.
#
# Corre sobre el árbol actual los escenarios de la definición de terminado de
# G4: build limpio, --test-run, consulta válida, código inválido, concurrencia
# sin zombis, SIGINT ignorada, cliente que corta a mitad, puerto ocupado y
# reinicio inmediato.
#
# Uso: scripts/acceptance.sh
# Requisitos: bash, make, gcc, ar y coreutils. No necesita netcat.
# Termina con 0 solo si todos los escenarios pasan.

set -uo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RAIZ" || exit 1

PUERTO=6666
TMP="$(mktemp -d)"
LOG="$TMP/servidor.log"
SRV_PID=""

total=0
fallos=0

ok()   { total=$((total + 1)); printf '  [OK]    %s\n' "$1"; }
mal()  { total=$((total + 1)); fallos=$((fallos + 1)); printf '  [FALLA] %s\n' "$1"; }
nota() { printf '          %s\n' "$1"; }
titulo() { printf '\n%s\n' "$1"; }

detener_servidor() {
    if [ -n "$SRV_PID" ] && kill -0 "$SRV_PID" 2>/dev/null; then
        kill -TERM "$SRV_PID" 2>/dev/null
        wait "$SRV_PID" 2>/dev/null
    fi
    SRV_PID=""
}

limpiar() {
    detener_servidor
    rm -rf "$TMP"
}
trap limpiar EXIT

puerto_ocupado() {
    if command -v ss >/dev/null 2>&1; then
        ss -tln 2>/dev/null | grep -q ":$PUERTO "
        return $?
    fi
    (exec 3<>"/dev/tcp/127.0.0.1/$PUERTO") 2>/dev/null
}

arrancar_servidor() {
    : > "$LOG"
    ./server >"$LOG" 2>&1 &
    SRV_PID=$!
    local i
    for i in $(seq 1 50); do
        if puerto_ocupado; then
            return 0
        fi
        if ! kill -0 "$SRV_PID" 2>/dev/null; then
            return 1
        fi
        sleep 0.1
    done
    return 1
}

# Envía bytes crudos y devuelve todo lo que responda el servidor hasta EOF.
crudo() {
    local datos="$1"
    exec 3<>"/dev/tcp/127.0.0.1/$PUERTO" || return 1
    printf '%b' "$datos" >&3
    cat <&3
    exec 3<&- 3>&-
}

# Envía bytes crudos y cierra sin leer: simula un cliente que corta a mitad.
crudo_y_corto() {
    local datos="$1"
    exec 3<>"/dev/tcp/127.0.0.1/$PUERTO" || return 1
    printf '%b' "$datos" >&3
    exec 3<&- 3>&-
}

lineas_ip() { grep -c '^IP ' "$LOG" 2>/dev/null || echo 0; }

zombis() {
    ps -eo stat=,ppid= --no-headers 2>/dev/null |
        awk -v p="$SRV_PID" '$2 == p && $1 ~ /^Z/ {n++} END {print n + 0}'
}

contiene() {
    case "$1" in
        *"$2"*) return 0 ;;
        *) return 1 ;;
    esac
}

# ---------------------------------------------------------------------------
titulo "1) Build limpio"
SALIDA="$(make clean 2>&1 && make all 2>&1)"
RC=$?
if [ "$RC" -ne 0 ]; then
    mal "make clean && make all termina con $RC"
    nota "$SALIDA"
else
    ok "make clean && make all termina con 0"
    if printf '%s' "$SALIDA" | grep -qi 'warning'; then
        mal "el build emite warnings"
        nota "$SALIDA" | head -5
    else
        ok "el build no emite warnings"
    fi
    for f in libmonitor.a server client; do
        if [ -f "$f" ]; then
            ok "existe $f en la raíz"
        else
            mal "falta $f en la raíz"
        fi
    done
    MIEMBROS="$(ar t libmonitor.a 2>/dev/null | tr '\n' ' ')"
    if contiene "$MIEMBROS" "bd.o" && contiene "$MIEMBROS" "episodios.o" &&
        contiene "$MIEMBROS" "formato.o"; then
        ok "libmonitor.a empaqueta bd.o, episodios.o y formato.o"
    else
        mal "libmonitor.a no empaqueta los tres objetos: $MIEMBROS"
    fi
fi

if [ ! -x ./server ] || [ ! -x ./client ]; then
    printf '\nNo hay binarios para probar. Abortando.\n'
    exit 1
fi

if puerto_ocupado; then
    printf '\nEl puerto %d ya está ocupado. Liberalo antes de correr la aceptación.\n' "$PUERTO"
    exit 1
fi

# ---------------------------------------------------------------------------
titulo "2) Prueba automática (--test-run)"
SALIDA="$(./server --test-run 2>&1)"
RC=$?
if [ "$RC" -eq 0 ] && contiene "$SALIDA" "test-run: OK"; then
    ok "--test-run termina con 0 con el puerto libre"
else
    mal "--test-run termina con $RC: $SALIDA"
fi

# ---------------------------------------------------------------------------
titulo "3) Consulta válida"
if arrancar_servidor; then
    ok "el servidor arranca y escucha en el puerto $PUERTO"
    if [ -s "$LOG" ]; then
        mal "el servidor imprime algo al arrancar"
        nota "$(cat "$LOG")"
    else
        ok "el servidor arranca sin imprimir nada"
    fi

    RESPUESTA="$(./client S01E01)"
    for esperado in "Codigo: S01E01" "Titulo: The Hedge Knight" \
        "Puntuacion: 9.2" "Votos: 45,120"; do
        if contiene "$RESPUESTA" "$esperado"; then
            ok "./client S01E01 responde \"$esperado\""
        else
            mal "./client S01E01 no responde \"$esperado\""
            nota "$RESPUESTA"
        fi
    done

    if grep -q '^IP 127\.0\.0\.1 PID [0-9]\+$' "$LOG"; then
        ok "el padre imprime \"IP <ip> PID <pid>\" por la conexión"
    else
        mal "el padre no imprime la línea IP/PID"
        nota "$(cat "$LOG")"
    fi
else
    mal "el servidor no arranca"
    nota "$(cat "$LOG")"
fi

# ---------------------------------------------------------------------------
titulo "4) Código inválido"
RESPUESTA="$(./client S99E99)"
if [ "$RESPUESTA" = "ERROR: episodio no encontrado" ]; then
    ok "./client S99E99 devuelve el error del servidor"
else
    mal "./client S99E99 devuelve: $RESPUESTA"
fi

for caso in 'S0\n' 'S01E01X\n' 's01e01\n' '\n'; do
    RESPUESTA="$(crudo "$caso")"
    if contiene "$RESPUESTA" "ERROR: episodio no encontrado"; then
        ok "el servidor rechaza la solicitud cruda $(printf '%q' "$caso")"
    else
        mal "el servidor no rechaza $(printf '%q' "$caso"): $RESPUESTA"
    fi
done

if ./client S01E0 >/dev/null 2>&1; then
    mal "./client S01E0 no valida la longitud en el cliente"
else
    ok "./client S01E0 rechaza la longitud antes de enviar"
fi

# ---------------------------------------------------------------------------
titulo "5) Concurrencia y cero zombis"
ANTES="$(lineas_ip)"
PIDS=()
SALIDAS="$TMP/concurrentes"
: > "$SALIDAS"
for i in $(seq 1 10); do
    (./client "S01E0$((i % 6 + 1))" >> "$SALIDAS" 2>&1) &
    PIDS+=($!)
done
for p in "${PIDS[@]}"; do wait "$p"; done
sleep 1

RESPONDIDOS="$(grep -c '^Codigo: S01E' "$SALIDAS")"
if [ "$RESPONDIDOS" -eq 10 ]; then
    ok "10 clientes concurrentes obtienen respuesta"
else
    mal "solo $RESPONDIDOS de 10 clientes concurrentes obtuvieron respuesta"
fi

DESPUES="$(lineas_ip)"
NUEVAS=$((DESPUES - ANTES))
if [ "$NUEVAS" -eq 10 ]; then
    ok "el padre imprime una línea IP/PID por cada cliente concurrente ($NUEVAS)"
else
    mal "el padre imprimió $NUEVAS líneas para 10 clientes"
fi

Z="$(zombis)"
if [ "$Z" -eq 0 ]; then
    ok "el servidor no acumula hijos zombi"
else
    mal "el servidor dejó $Z hijos en estado zombi"
fi

# ---------------------------------------------------------------------------
titulo "6) SIGINT ignorada"
kill -INT "$SRV_PID" 2>/dev/null
sleep 0.3
if kill -0 "$SRV_PID" 2>/dev/null; then
    ok "el servidor sigue vivo tras SIGINT"
    if contiene "$(./client S01E04)" "Codigo: S01E04"; then
        ok "el servidor sigue atendiendo tras SIGINT"
    else
        mal "el servidor no atiende después de SIGINT"
    fi
else
    mal "SIGINT terminó el servidor"
fi

# ---------------------------------------------------------------------------
titulo "7) Cliente que corta a mitad"
crudo_y_corto 'S01'
crudo_y_corto ''
sleep 0.5
if kill -0 "$SRV_PID" 2>/dev/null; then
    ok "el servidor sobrevive a clientes que cortan sin leer"
else
    mal "el servidor murió por un cliente que cortó sin leer"
fi
if contiene "$(./client S01E06)" "Codigo: S01E06"; then
    ok "el servidor sigue atendiendo tras los cortes"
else
    mal "el servidor no atiende tras los cortes"
fi

# ---------------------------------------------------------------------------
titulo "8) Puerto ocupado"
SALIDA="$(./server --test-run 2>&1)"
RC=$?
if [ "$RC" -ne 0 ]; then
    ok "--test-run falla con el puerto ocupado (exit=$RC)"
else
    mal "--test-run devuelve 0 con el puerto ocupado"
fi

# ---------------------------------------------------------------------------
titulo "9) Reinicio inmediato"
detener_servidor
if arrancar_servidor; then
    ok "el servidor vuelve a arrancar inmediatamente después de SIGTERM"
    if contiene "$(./client S01E05)" "Codigo: S01E05"; then
        ok "el servidor reiniciado responde"
    else
        mal "el servidor reiniciado no responde"
    fi
else
    mal "el servidor no reinicia: el puerto quedó tomado"
    nota "$(cat "$LOG")"
fi
detener_servidor

# ---------------------------------------------------------------------------
printf '\n'
if [ "$fallos" -eq 0 ]; then
    printf 'Aceptación OK: %d comprobaciones.\n' "$total"
    exit 0
fi
printf 'Aceptación FALLIDA: %d de %d comprobaciones.\n' "$fallos" "$total"
exit 1
