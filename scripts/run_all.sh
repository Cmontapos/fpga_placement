#!/usr/bin/env bash
# Corre el placer sobre todos los benchmarks con varias semillas, valida cada
# resultado con el evaluador y guarda todo en results/results.csv.
#
# Uso:  scripts/run_all.sh                 # semillas 1..5, todos los benchmarks
#       scripts/run_all.sh "1 2 3" FPGA00  # semillas y benchmarks específicos
#       PAR=5 scripts/run_all.sh           # corre las semillas de cada benchmark en paralelo
#
# A cada benchmark se le pasa su tiempo máximo del enunciado (--time-limit);
# el placer calibra sus movimientos para usar ~50 % de ese tiempo.
set -u
cd "$(dirname "$0")/.."

SEEDS=${1:-"1 2 3 4 5"}
shift || true
BENCHES=${*:-$(ls benchmarks)}

# Metas de HPWL del enunciado (solo para el reporte, el algoritmo no las usa)
declare -A TARGET=([FPGA00]=52880 [FPGA01]=1453168 [FPGA02]=2716304 [FPGA03]=11528160 [FPGA04]=21846668 [FPGA05]=38891148)
# Tiempo máximo del enunciado, en segundos (restricción del problema, no un parámetro de ajuste)
declare -A LIMIT=([FPGA00]=300 [FPGA01]=360 [FPGA02]=900 [FPGA03]=9000 [FPGA04]=14400 [FPGA05]=18000)
PAR=${PAR:-1}

make -s placer || exit 1
mkdir -p results outputs
CSV=results/results.csv
[ -f "$CSV" ] || echo "fecha,benchmark,seed,hpwl,time_s,integrity,legality" > "$CSV"
STAMP=$(date +%Y-%m-%dT%H:%M:%S)

run_one() {
    # Cada llamada corre en su propio subproceso (&), así que las variables no se mezclan
    local b=$1 s=$2 d=benchmarks/$1 out log res hpwl t ev integ legal lim
    out=outputs/$b/new_${b}_seed$s.pl
    log=outputs/$b/log_${b}_seed$s.txt
    mkdir -p "outputs/$b"
    echo "[$(date +%H:%M:%S)] corriendo $b semilla $s ... (progreso: tail -f $log)"
    lim=()
    [ -n "${LIMIT[$b]:-}" ] && lim=(--time-limit "${LIMIT[$b]}")
    ./placer "$d/$b.sitemap" "$d/$b.nets" "$d/$b.pl" "$out" --seed "$s" "${lim[@]}" > "$log" 2>&1
    res=$(grep '^RESULT' "$log")
    hpwl=$(sed -E 's/.*hpwl=([0-9]+).*/\1/' <<< "$res")
    t=$(sed -E 's/.*time=([0-9.]+).*/\1/' <<< "$res")
    ev=$(./evaluator "$d/$b.sitemap" "$d/$b.nets" "$d/$b.pl" "$out" 2>&1)
    integ=$(grep -q 'integrity ...... PASS' <<< "$ev" && echo PASS || echo FAIL)
    legal=$(grep -q 'legality ...... PASS' <<< "$ev" && echo PASS || echo FAIL)
    echo "$STAMP,$b,$s,$hpwl,$t,$integ,$legal" >> "$CSV"
    printf "%-7s seed %-3s HPWL %10s  %8ss  %s/%s\n" "$b" "$s" "$hpwl" "$t" "$integ" "$legal"
}

for b in $BENCHES; do
    n=0
    for s in $SEEDS; do
        run_one "$b" "$s" &
        n=$((n + 1))
        if [ "$n" -ge "$PAR" ]; then wait; n=0; fi
    done
    wait
    # La mejor semilla LEGAL de esta corrida pasa a ser el placement de entrega
    best=$(grep "^$STAMP,$b," "$CSV" | grep ',PASS,PASS$' | sort -t, -k4,4n | head -1 | cut -d, -f3)
    if [ -n "$best" ]; then
        cp "outputs/$b/new_${b}_seed$best.pl" "outputs/$b/new_$b.pl"
        cp "outputs/$b/log_${b}_seed$best.txt" "outputs/$b/log_$b.txt"
        echo "$b: mejor semilla $best -> outputs/$b/new_$b.pl"
    fi
done

echo
echo "Resumen de esta corrida ($STAMP):"
printf "%-7s %12s %12s %10s %10s %12s %s\n" Bench Mejor Promedio "Desv.std" "t_prom(s)" Meta Estado
for b in $BENCHES; do
    grep "^$STAMP,$b," "$CSV" | awk -F, -v b="$b" -v meta="${TARGET[$b]:-0}" '
        { n++; h=$4; s+=h; ss+=h*h; t+=$5; if (n==1 || h<best) best=h; if ($6!="PASS"||$7!="PASS") bad=1 }
        END {
            if (n==0) exit;
            m=s/n; v=ss/n-m*m; if (v<0) v=0;
            estado = bad ? "ILEGAL" : (meta>0 && best<meta ? "cumple meta" : "no cumple");
            printf "%-7s %12d %12.0f %10.0f %10.2f %12d %s\n", b, best, m, sqrt(v), t/n, meta, estado
        }'
done
