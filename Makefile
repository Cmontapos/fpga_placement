# Placement para FPGAs — IE0411
# Uso rápido (ver README.md, sección "Cómo usarlo"):
#   make                         compila ./placer
#   make run BENCH=FPGA01        corre un benchmark con su tiempo límite y lo valida con el evaluador
#   make run-all                 corre todos los benchmarks (semilla 1) y los valida
#   make experiments             5 semillas por benchmark, CSV y resumen (lo que pide el informe)
#   make figures                 imágenes del placement y curvas de convergencia
#   make selftest BENCH=FPGA04   verifica el HPWL incremental
#   make help                    lista completa

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O3 -march=native -Wall -Wextra
DBGFLAGS  = -std=c++17 -O0 -g -Wall -Wextra -fsanitize=address,undefined

SRC = $(wildcard src/*.cpp)
HDR = $(wildcard src/*.h)

# ---- Parámetros de ejecución (se pueden cambiar en la línea de comandos) ----
BENCH   ?= FPGA01
SEED    ?= 1
SEEDS   ?= 1 2 3 4 5
PAR     ?= 1
BENCHES ?= $(sort $(notdir $(wildcard benchmarks/FPGA*)))

# Tiempo máximo de cada benchmark según el enunciado, en segundos. Es una
# restricción del problema, no un parámetro de ajuste: el placer calibra sus
# movimientos para usar ~50 % de este tiempo. Para un benchmark que no esté
# en la lista no se pasa límite (se usa inner_num = 1).
LIMIT_FPGA00 = 300
LIMIT_FPGA01 = 360
LIMIT_FPGA02 = 900
LIMIT_FPGA03 = 9000
LIMIT_FPGA04 = 14400
LIMIT_FPGA05 = 18000
TIME_LIMIT ?= $(LIMIT_$(BENCH))

B   = benchmarks/$(BENCH)/$(BENCH)
OUT ?= outputs/$(BENCH)/new_$(BENCH).pl

# ---- Compilación --------------------------------------------------------------
placer: $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

# Versión con chequeos de memoria, para depurar
placer_dbg: $(SRC) $(HDR)
	$(CXX) $(DBGFLAGS) -o $@ $(SRC)

# ---- Ejecución ------------------------------------------------------------------
# Corre un benchmark y valida el resultado con el evaluador del curso.
run: placer
	@test -f $(B).sitemap || { echo "No existe $(B).sitemap"; exit 1; }
	@mkdir -p $(dir $(OUT))
	./placer $(B).sitemap $(B).nets $(B).pl $(OUT) --seed $(SEED) $(if $(TIME_LIMIT),--time-limit $(TIME_LIMIT))
	./evaluator $(B).sitemap $(B).nets $(B).pl $(OUT)

# Solo el evaluador sobre una salida ya generada
eval:
	./evaluator $(B).sitemap $(B).nets $(B).pl $(OUT)

# Todos los benchmarks, una semilla, uno detrás de otro
run-all: placer
	@for b in $(BENCHES); do $(MAKE) --no-print-directory run BENCH=$$b SEED=$(SEED) || exit 1; done

# Experimentos del informe: varias semillas por benchmark, validación,
# results/results.csv y tabla resumen. PAR=n corre n semillas a la vez.
experiments: placer
	PAR=$(PAR) scripts/run_all.sh "$(SEEDS)" $(BENCHES)

# Figuras: placement inicial vs final y curva de convergencia, para cada
# benchmark que tenga salida (outputs/FPGAxx/new_FPGAxx.pl y su log).
figures:
	@mkdir -p figuras
	@for b in $(BENCHES); do \
	  pl=outputs/$$b/new_$$b.pl; [ -f $$pl ] || continue; \
	  python3 scripts/plot_placement.py placement benchmarks/$$b/$$b.sitemap benchmarks/$$b/$$b.pl $$pl figuras/placement_$$b.png --titulo $$b; \
	  log=$$(ls outputs/$$b/log_$$b.txt 2>/dev/null || ls outputs/$$b/log_$${b}_seed*.txt 2>/dev/null | head -1); \
	  [ -n "$$log" ] && python3 scripts/plot_placement.py curva $$log figuras/curva_$$b.png --titulo $$b; \
	done; true

# Prueba del HPWL incremental contra un recálculo completo
selftest: placer
	./placer $(B).sitemap $(B).nets $(B).pl /dev/null --selftest 2000000

clean:
	rm -f placer placer_dbg

help:
	@echo "make                         compila ./placer"
	@echo "make run BENCH=FPGA01        corre un benchmark (SEED=1, tiempo límite del enunciado) y lo evalúa"
	@echo "make run BENCH=FPGA04 TIME_LIMIT=   sin presupuesto de tiempo (rápido, inner_num = 1)"
	@echo "make eval BENCH=FPGA01       evalúa outputs/FPGA01/new_FPGA01.pl"
	@echo "make run-all                 todos los benchmarks con SEED=1"
	@echo "make experiments PAR=5       5 semillas por benchmark (SEEDS=\"1 2 3 4 5\"), CSV y resumen"
	@echo "make figures                 figuras en figuras/"
	@echo "make selftest BENCH=FPGA04   prueba del HPWL incremental"
	@echo "make placer_dbg              versión con sanitizers"
	@echo "make clean                   borra los ejecutables"

.PHONY: run eval run-all experiments figures selftest clean help
