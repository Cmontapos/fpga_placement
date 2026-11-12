#!/usr/bin/env bash
# Experimento de ablación: mide cuánto aporta cada pieza del algoritmo,
# corriendo varias configuraciones en paralelo y validando con el evaluador.
#
# Uso:  scripts/ablation.sh <salida.csv> "<semillas>" "<benchmarks>" <paralelo> cfg1 cfg2 ...
# Cada cfg es  nombre=binario:opciones   (opciones separadas por comas), p. ej.
#       base=./placer_fase1:  alfa=./placer:--no-window  ventana=./placer:--alpha,0.95  ambas=./placer:
# Escribe una fila por corrida en el CSV:  config,benchmark,seed,hpwl,time_s,legal
set -u
cd "$(dirname "$0")/.."
CSV=$1; SEEDS=$2; BENCHES=$3; PAR=$4; shift 4
OUT=outputs/ablation
mkdir -p "$OUT" "$(dirname "$CSV")"
[ -f "$CSV" ] || echo "config,benchmark,seed,hpwl,time_s,legal" > "$CSV"

run_one() {  # config binario opciones benchmark semilla
    local cfg=$1 bin=$2 opts=${3//,/ } b=$4 s=$5
    [ "$opts" = "-" ] && opts=""
    local d=benchmarks/$b pl=$OUT/${b}_${cfg}_seed$s.pl log=$OUT/${b}_${cfg}_seed$s.log
    # shellcheck disable=SC2086
    "$bin" "$d/$b.sitemap" "$d/$b.nets" "$d/$b.pl" "$pl" --seed "$s" $opts > "$log" 2>&1
    local res hpwl t legal
    res=$(grep '^RESULT' "$log")
    hpwl=$(sed -E 's/.*hpwl=([0-9]+).*/\1/' <<< "$res")
    t=$(sed -E 's/.*time=([0-9.]+).*/\1/' <<< "$res")
    legal=$(./evaluator "$d/$b.sitemap" "$d/$b.nets" "$d/$b.pl" "$pl" 2>&1 | grep -c 'PASS')
    [ "$legal" = 2 ] && legal=PASS || legal=FAIL
    echo "$cfg,$b,$s,$hpwl,$t,$legal" | tee -a "$CSV"
}
export -f run_one
export OUT CSV

for b in $BENCHES; do for c in "$@"; do for s in $SEEDS; do
    name=${c%%=*}; rest=${c#*=}; bin=${rest%%:*}; opts=${rest#*:}
    printf '%s\n' "$name" "$bin" "${opts:--}" "$b" "$s"
done; done; done | xargs -P "$PAR" -d '\n' -n 5 bash -c 'run_one "$@"' _
