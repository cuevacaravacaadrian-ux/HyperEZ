#!/bin/sh
# Ejecuta bench.hz varias veces y muestra el mínimo de cada métrica (ms).
EXE=${1:-./build/hyper.exe}
N=${2:-7}
for i in $(seq $N); do "$EXE" tests/bench/bench.hz | tail -1; done | awk '
{ for (i = 1; i <= NF; i++) if ($i == "ms") { k = $(i-2); v = $(i-1); if (!(k in m) || v < m[k]) m[k] = v; order[k] = i } }
END { for (k in m) printf "%s=%s ", k, m[k]; print "" }'
