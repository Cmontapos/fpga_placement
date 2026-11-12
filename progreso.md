# Bitácora de progreso: Placement para FPGAs (IE0411)

Cada fila registra un cambio o decisión, su motivo y su resultado. Ver [contexto.md](contexto.md) para el resumen completo del proyecto.

## Estado por fase

| Fase | Contenido | Estado |
|---|---|---|
| 0. Ambiente | WSL, herramientas, esqueleto compilando | ✅ `make`, PASS/PASS en los 5 benchmarks, selftest PASS |
| 1. Annealing básico | α fijo, sin ventana | ✅ FPGA00, FPGA01 y FPGA02 bajo la meta |
| 2. Escalar | α adaptativo, ventana, M según tamaño | ✅ ablación hecha; FPGA03/04 bajo la meta |
| 3. Mejoras | Placement inicial constructivo, refinamiento greedy | 🔄 greedy hecho (aporte mínimo) y presupuesto de tiempo; constructivo no implementado (posible mejora) |
| 4. Robustez | Sin parámetros por benchmark; 5 semillas | ✅ 25/25 corridas PASS/PASS y bajo la meta; presupuesto de tiempo con freno |
| 5. Entrega | Tablas, informe, presentación | ✅ README = informe; Makefile con `run`, `experiments`, `figures`; presentación de 15 láminas; repositorio git listo (falta el primer commit y subirlo a GitHub) |

## Bitácora de decisiones

| Fecha | Cambio o decisión | Motivo | Resultado (HPWL / tiempo) | ¿Se queda? |
|---|---|---|---|---|
| 2026-10-27 | Esqueleto con HPWL incremental | La sección 7 del enunciado pide evaluación eficiente | Coincide con el evaluador; ~3 µs por movimiento | Sí |
| 2026-11-08 | Evaluador corrido de FPGA00 contra sí mismo | Comprobar que el entorno y el binario funcionan | integrity PASS, legality PASS, HPWL 438681 en ambos, diff 0,00 % | Sí |
| 2026-11-08 | Se acepta GitHub privado para sincronizar el código | Permite ramas, historial y revisión | Pendiente de crear el repositorio | Sí |
| 2026-11-08 | `chmod +x evaluator` y limpieza de archivos de Windows/macOS | El evaluador perdió el permiso de ejecución al copiarlo desde Windows y `run_all.sh` marcaba FAIL/FAIL falsos | PASS/PASS en FPGA00–04; selftest de 200 000 movimientos PASS | Sí |
| 2026-11-08 | Fase 1: annealing básico (T0 = 20σ, α = 0,95 fijo, sin ventana, M = 1·N^(4/3), parada T < 0,005·HPWL/nets) | Primera versión completa del algoritmo, para medir la base antes de agregar mejoras | FPGA00: 25 809–38 437 (prom. 31 621), 0,4 s · FPGA01: 740 738, 66 s · FPGA02: 1 198 944, 203 s · FPGA03: 3 987 586, 918 s · FPGA04: 6 477 992, 707 s (semilla 1). Todos PASS/PASS y bajo la meta | Sí |
| 2026-11-10 | Hallazgo: en `outputs/FPGA01..04` los `.pl` de semillas 2–5 son de la corrida de las 20:53 del 8/11 (antes del algoritmo) | Revisión de fechas de archivos | Son el placement inicial aleatorio; hay que regenerarlos con el algoritmo final | — |
| 2026-11-10 | Fase 2: ventana adaptativa (rlim, objetivo de aceptación 0,44) con índice de sitios por columna; RAM/DSP siempre alcanzan la columna vecina de su tipo | Con T baja, los movimientos lejanos casi siempre se rechazan | Ablación (`results/ablation_fase2.csv`, 3 semillas, prom.): FPGA01 740 092 → 442 756 (−40 %); FPGA02 1 165 883 → 770 675 (−34 %) | Sí |
| 2026-11-10 | Fase 2: α adaptativo de VPR (0,5 / 0,9 / 0,95 / 0,8 según R) | Gastar el tiempo donde se mejora | **Solo, sin ventana, empeora** (FPGA01 1 065 080; FPGA02 1 607 203): sin ventana R cae rápido y la tabla enfría de golpe. **Con ventana** da la misma calidad que la ventana sola y ~35 % menos tiempo (FPGA01 442 978; FPGA02 783 755) | Sí (junto con la ventana) |
| 2026-11-10 | Fase 3: refinamiento greedy final (T = 0, Δ ≤ 0, pasadas hasta mejorar < 0,01 %) | Pulir el resultado | FPGA01: 442 118 → 442 116 (despreciable): el annealing ya termina frío. Cuesta < 1 % del tiempo | Sí (no estorba) |
| 2026-11-10 | Tiempo real FPGA01 con fase 2 (corrida sola) | Los tiempos de la ablación están inflados ~3× por correr 12 procesos en paralelo | 45 s (vs 66 s de la fase 1) con HPWL 442 116 | — |
| 2026-11-10 | Fase 2 en FPGA03/04 (inner = 1, semilla 1) | Medir los grandes | FPGA03: 3 987 586 → 3 081 084 (−23 %); FPGA04: 6 477 992 → 5 483 726 (−15 %). PASS/PASS. ~160 temperaturas en todos los benchmarks | Sí |
| 2026-11-10 | Barrido de inner_num en FPGA01 (`results/inner_fase2.csv`) | Curva calidad contra tiempo | inner 1: 442 116 · 2: 415 015 (−6 %) · 4: 396 581 (−10 %) · 8: 366 006 (−17 %); el tiempo crece lineal | — |
| 2026-11-11 | Se confirma que la única competencia es de HPWL; el tiempo solo debe quedar dentro del límite | Texto del enunciado | Conviene usar el tiempo disponible para bajar HPWL | — |
| 2026-11-11 | Presupuesto de tiempo `--time-limit S`: la 1.ª temperatura mide el costo por movimiento y M se calibra para usar ~50 % del límite (165 temperaturas planeadas); freno de seguridad al 80 % | Usar el tiempo sin parámetros por benchmark: el límite es una restricción del enunciado, y la calibración funciona igual en FPGA05 y en una máquina más lenta | Calibración: meta 10 s → 10,9 s real. Freno probado (FPGA03 con límite de 3 s se detiene y queda legal). `run_all.sh` pasa el límite de cada benchmark y acepta `PAR=n` | Sí |
| 2026-11-11 | FPGA00 con presupuesto completo (150 s, 3 semillas) | Probar el presupuesto | 10 401 – 12 026 (prom. 11 426); con 10 s había dado 10 376: FPGA00 ya está en su meseta y domina la variación entre semillas. Posible mejora: reinicios múltiples en benchmarks chicos | — |
| 2026-11-11 | El profesor aprobó que el proyecto se haga de forma individual | El enunciado pide grupos de 2 a 3 | Aprobado | — |
| 2026-11-11 | Corrida final: 5 semillas × 5 benchmarks con presupuesto de tiempo (`PAR=5`) | Resultados para el informe y `.pl` de entrega | 25/25 PASS/PASS; ver tabla de resultados | Sí |
| | | | | |

## Resultados por benchmark

**Versión final** (fase 2 + greedy + presupuesto de tiempo), corrida del 2026-11-11 08:50 con `PAR=5 scripts/run_all.sh`: 5 semillas por benchmark en paralelo, validadas con el evaluador. Archivo: `results/corrida_final_20261111_0850.txt`. El mejor `.pl` de cada benchmark se copió a `outputs/FPGA0X/new_FPGA0X.pl`. Figuras en `figuras/` (`scripts/plot_placement.py`).

| Benchmark | Meta HPWL | Mejor HPWL (semilla) | Promedio ± desv. (5 semillas) | Tiempo prom. / límite | Veces bajo la meta | Legal |
|---|---|---|---|---|---|---|
| FPGA00 | 52 880 | 11 434 (3) | 11 544 ± 183 | 151 s / 300 s | 4,6× | 5/5 PASS |
| FPGA01 | 1 453 168 | 368 386 (5) | 376 729 ± 7 873 | 193 s / 360 s | 3,9× | 5/5 PASS |
| FPGA02 | 2 716 304 | 699 386 (4) | 709 234 ± 6 756 | 493 s / 900 s | 3,9× | 5/5 PASS |
| FPGA03 | 11 528 160 | 2 870 794 (4) | 2 942 777 ± 41 498 | 4 036 s / 9 000 s | 4,0× | 5/5 PASS |
| FPGA04 | 21 846 668 | 5 308 558 (2) | 5 385 052 ± 48 638 | 7 246 s / 14 400 s | 4,1× | 5/5 PASS |

Nota: con 5 procesos en paralelo el costo por movimiento sube (FPGA04: ~4,6 µs/mov contra ~1,9 solo), así que la calibración dio inner_num ≈ 4,5 en FPGA03 y ≈ 5,6 en FPGA04. Corriendo un solo proceso el presupuesto alcanzaría para más movimientos.

### Evolución por versión (semilla 1 o mejor de 5)

| Benchmark | Inicial | Fase 1 | Fase 2 (inner 1) | Final |
|---|---|---|---|---|
| FPGA00 | 438 681 | 25 809 | 19 284 | 11 434 |
| FPGA01 | 12 649 411 | 740 738 | 442 116 | 368 386 |
| FPGA02 | 19 264 983 | 1 198 944 | 773 713 | 699 386 |
| FPGA03 | 53 266 524 | 3 987 586 | 3 081 084 | 2 870 794 |
| FPGA04 | 64 917 561 | 6 477 992 | 5 483 726 | 5 308 558 |

## Pendientes

- [x] Evaluador en FPGA01 a FPGA04 (PASS/PASS con el placement inicial)
- [x] Verificar `make` y `scripts/run_all.sh "1" FPGA00` (el proyecto quedó en `~/placement`)
- [ ] Crear el repositorio privado en GitHub (el repo local ya está inicializado; falta configurar git, hacer el primer commit y subirlo)
- [ ] Confirmar con el profesor la fecha de entrega
- [x] Confirmar con el profesor si se puede trabajar solo: **aprobado**
- [x] Implementar `runPlacement()` (Fase 1)
- [x] Fase 2: α adaptativo y ventana; medir FPGA03 y FPGA04
- [x] Corrida final de 5 semillas, figuras, README/informe y presentación
- [x] Poner el nombre del autor en el README y la portada de la presentación

## Uso de IA (para declarar en el informe)

Se usa un asistente de IA como apoyo: propone opciones, explica y ayuda a programar. El estudiante decide, prueba y juzga los resultados.
