// placer.h
// Algoritmo de placement: simulated annealing (ver README.md).
#pragma once

#include <cstdint>
#include <random>

#include "design.h"
#include "hpwl.h"

struct Options {
    uint64_t seed = 1;          // semilla para reproducibilidad
    long long selftest = 0;     // > 0: corre la prueba del HPWL incremental con ese número de movimientos

    // Parámetros del annealing. Son los mismos para todos los benchmarks;
    // se pueden cambiar desde la línea de comandos solo para hacer experimentos.
    double innerNum = 1.0;      // movimientos por temperatura = innerNum * N^(4/3)
    bool adaptiveAlpha = true;  // alfa según la tasa de aceptación (tabla de VPR)
    double alpha = 0.95;        // alfa fijo, solo si adaptiveAlpha = false (--alpha A)
    bool useWindow = true;      // ventana de movimiento adaptativa (rlim de VPR)
    bool greedy = true;         // refinamiento greedy (T = 0) al final del annealing
    double timeLimit = 0;       // tiempo máximo del benchmark en segundos (--time-limit); 0 = sin presupuesto,
                                // se usa innerNum. Con límite, M se calibra para usar ~50 % de él.
};

// Algoritmo principal: modifica d.cellX / d.cellY (siempre a través de hp.proposeMove + hp.commit,
// o llamando hp.rebuild() después de cambios directos) y debe dejar un placement legal.
void runPlacement(Design& d, Hpwl& hp, std::mt19937_64& rng, const Options& opt);

// Prueba: hace movimientos aleatorios legales y compara el HPWL incremental
// contra un recálculo completo. Devuelve true si todo coincide.
bool selfTest(Design& d, Hpwl& hp, std::mt19937_64& rng, long long moves);
