// placer.cpp
#include "placer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

// ===========================================================================
// ALGORITMO DE PLACEMENT: simulated annealing con enfriamiento adaptativo y
// ventana de movimiento (rlim), al estilo de VPR, seguido de un refinamiento
// greedy. Ver las secciones 3 y 6 del README.md.
//
// Interfaz con el resto del programa:
//   hp.canMoveTo(c, x, y)     -> ¿movimiento legal?
//   hp.proposeMove(c, x, y)   -> delta de HPWL (mover o intercambiar), sin aplicar
//   hp.commit()               -> aplica la última propuesta
//   hp.total()                -> HPWL actual
// ===========================================================================

namespace {

using Clock = std::chrono::steady_clock;

// Índice de sitios por tipo, organizado por columnas:
//   cols[t]      = columnas (x) que tienen al menos un sitio de tipo t, ordenadas
//   colYs[t][i]  = filas (y) de los sitios de tipo t en la columna cols[t][i], ordenadas
// Sirve para escoger rápido un sitio del tipo correcto DENTRO de una ventana.
// No supone nada de la geometría de un benchmark en particular: se construye
// a partir del .sitemap.
struct SiteIndex {
    std::vector<std::vector<int>> cols;
    std::vector<std::vector<std::vector<int>>> colYs;

    explicit SiteIndex(const Design& d) : cols(NUM_SITE_TYPES), colYs(NUM_SITE_TYPES) {
        for (int t = 0; t < NUM_SITE_TYPES; ++t) {
            // sitesOfType está ordenado por índice de sitio = x * H + y,
            // o sea, agrupado por columna y con y creciente.
            for (int s : d.sitesOfType[t]) {
                int x = d.siteX(s), y = d.siteY(s);
                if (cols[t].empty() || cols[t].back() != x) {
                    cols[t].push_back(x);
                    colYs[t].emplace_back();
                }
                colYs[t].back().push_back(y);
            }
        }
    }
};

// Rango [lo, hi) de los valores de v (ordenado) dentro de [a - r, a + r].
// Si el rango tiene un solo elemento o ninguno, se amplía al vecino más
// cercano de cada lado. Así una RAM o DSP siempre puede llegar a la columna
// vecina de su tipo aunque la ventana sea más angosta que la separación
// entre columnas (README.md, sección 4).
inline void windowRange(const std::vector<int>& v, int a, int r, int& lo, int& hi) {
    const int n = (int)v.size();
    lo = (int)(std::lower_bound(v.begin(), v.end(), a - r) - v.begin());
    hi = (int)(std::upper_bound(v.begin(), v.end(), a + r) - v.begin());
    if (hi - lo <= 1) {
        if (lo >= n) lo = n - 1;  // a la derecha de todo: el último
        if (hi <= lo) hi = lo + 1;
        lo = std::max(0, lo - 1);
        hi = std::min(n, hi + 1);
    }
}

// Escoge un sitio del mismo tipo que la celda c dentro de la ventana de
// radio r alrededor de ella (primero una columna, luego una fila de esa
// columna). Devuelve false si no encontró un sitio distinto al actual.
inline bool pickTarget(const Design& d, const SiteIndex& idx, int c, int r, std::mt19937_64& rng, int& x,
                       int& y) {
    const int t = d.cellType[c], cx = d.cellX[c], cy = d.cellY[c];
    const auto& cols = idx.cols[t];
    for (int attempt = 0; attempt < 4; ++attempt) {
        int clo, chi;
        windowRange(cols, cx, r, clo, chi);
        int ci = std::uniform_int_distribution<int>(clo, chi - 1)(rng);
        const auto& ys = idx.colYs[t][ci];
        int ylo, yhi;
        windowRange(ys, cy, r, ylo, yhi);
        x = cols[ci];
        y = ys[std::uniform_int_distribution<int>(ylo, yhi - 1)(rng)];
        if (x != cx || y != cy) return true;
    }
    return false;
}

// Factor de enfriamiento adaptativo de VPR (Betz & Rose): enfría rápido cuando
// casi todo se acepta (es puro azar) o casi nada (ya está congelado), y
// despacio en la zona intermedia, donde más se mejora.
inline double adaptiveAlpha(double R) {
    if (R > 0.96) return 0.5;
    if (R > 0.8) return 0.9;
    if (R > 0.15) return 0.95;
    return 0.8;
}

}  // namespace

void runPlacement(Design& d, Hpwl& hp, std::mt19937_64& rng, const Options& opt) {
    const auto t0 = Clock::now();
    auto elapsed = [&] { return std::chrono::duration<double>(Clock::now() - t0).count(); };

    // ---- Celdas móviles (las IO fijas nunca se tocan) ---------------------
    std::vector<int> movable;
    for (int c = 0; c < d.numCells; ++c)
        if (!d.cellFixed[c]) movable.push_back(c);
    if (movable.empty()) return;
    const double N = (double)movable.size();
    std::uniform_int_distribution<size_t> pickCell(0, movable.size() - 1);
    std::uniform_real_distribution<double> unif(0.0, 1.0);

    const SiteIndex idx(d);
    const double rlimMax = std::max(d.W, d.H);  // la ventana empieza cubriendo todo el chip
    double rlim = rlimMax;

    // ---- Temperatura inicial: T0 = 20 * desviación estándar de los delta --
    // Se proponen N movimientos al azar SIN aplicarlos y se mide qué tanto
    // varía el HPWL. Así T0 se ajusta sola a la escala de cada benchmark.
    double sum = 0, sum2 = 0;
    long long samples = 0;
    for (long long i = 0; i < (long long)N; ++i) {
        int c = movable[pickCell(rng)], x, y;
        if (!pickTarget(d, idx, c, (int)rlim, rng, x, y) || !hp.canMoveTo(c, x, y)) continue;
        double delta = (double)hp.proposeMove(c, x, y);
        sum += delta;
        sum2 += delta * delta;
        ++samples;
    }
    double mean = samples ? sum / samples : 0;
    double sigma = samples ? std::sqrt(std::max(0.0, sum2 / samples - mean * mean)) : 1;
    double T = 20.0 * std::max(sigma, 1.0);

    // ---- Movimientos por temperatura: M = innerNum * N^(4/3) (VPR) --------
    long long M = std::max(1LL, (long long)(opt.innerNum * std::pow(N, 4.0 / 3.0)));

    // ---- Presupuesto de tiempo (--time-limit) ------------------------------
    // Si se da el tiempo máximo del benchmark, la primera temperatura se usa
    // para medir cuánto cuesta un movimiento y M se escoge para que el
    // annealing completo use cerca de kTargetFrac del límite. Con el programa
    // de enfriamiento adaptativo el annealing usa ~160 temperaturas en todos
    // los benchmarks (medido en FPGA01-04), así que se planea con kPlanTemps.
    // Freno de seguridad: si se llega a kGuardFrac del límite, el annealing se
    // detiene (el placement siempre es legal, así que se puede cortar en
    // cualquier momento).
    const double kTargetFrac = 0.5, kGuardFrac = 0.8, kPlanTemps = 165;
    const bool budgeted = opt.timeLimit > 0;

    std::printf("Annealing fase 2: N=%.0f  M=%lld movs/temp  T0=%.1f  alfa=%s  ventana=%s\n", N, M, T,
                opt.adaptiveAlpha ? "adaptativo" : std::to_string(opt.alpha).c_str(),
                opt.useWindow ? "adaptativa" : "todo el chip");

    // ---- Ciclo principal ---------------------------------------------------
    // Para cuando T es diminuta comparada con el costo promedio de una net.
    int iter = 0;
    long long totalMoves = 0;
    while (T > 0.005 * (double)hp.total() / d.numNets) {
        if (budgeted && elapsed() > kGuardFrac * opt.timeLimit) {
            std::printf("  FRENO: se alcanzó el %.0f%% del tiempo límite; se detiene el annealing\n", 100 * kGuardFrac);
            break;
        }
        const double tIter = elapsed();
        const int r = std::max(1, (int)rlim);
        long long accepted = 0;
        for (long long m = 0; m < M; ++m) {
            int c = movable[pickCell(rng)], x, y;
            if (!pickTarget(d, idx, c, r, rng, x, y) || !hp.canMoveTo(c, x, y)) continue;
            long long delta = hp.proposeMove(c, x, y);
            // Criterio de Metropolis
            if (delta <= 0 || unif(rng) < std::exp(-(double)delta / T)) {
                hp.commit();
                ++accepted;
            }
        }
        totalMoves += M;
        double R = (double)accepted / M;
        std::printf("  it=%3d  T=%12.3f  HPWL=%11lld  R=%.4f  rlim=%6.1f  t=%8.1fs\n", iter, T, hp.total(), R, rlim,
                    elapsed());
        std::fflush(stdout);

        // Enfriar
        T *= opt.adaptiveAlpha ? adaptiveAlpha(R) : opt.alpha;
        // Ajustar la ventana para mantener la aceptación cerca de 0.44 (Lam y Delosme)
        if (opt.useWindow) rlim = std::min(rlimMax, std::max(1.0, rlim * (1.0 - 0.44 + R)));

        // Calibración: después de la primera temperatura se conoce el costo
        // real de un movimiento en esta máquina y en este benchmark.
        if (budgeted && iter == 0) {
            const double perMove = (elapsed() - tIter) / M;
            const double remaining = kTargetFrac * opt.timeLimit - elapsed();
            M = std::max(1LL, (long long)(remaining / (kPlanTemps * perMove)));
            std::printf("  presupuesto: %.0f s de %.0f s  costo=%.3f us/mov  ->  M=%lld (inner_num=%.2f)\n",
                        kTargetFrac * opt.timeLimit, opt.timeLimit, perMove * 1e6, M, M / std::pow(N, 4.0 / 3.0));
        }
        ++iter;
    }
    std::printf("Annealing terminado: %d temperaturas, %lld movimientos, %.1f s\n", iter, totalMoves, elapsed());

    // ---- Refinamiento greedy final (T = 0) ---------------------------------
    // Solo se aceptan movimientos que no empeoran (Δ <= 0; los Δ = 0 dejan
    // "deslizarse" por zonas planas). Se repiten pasadas de M movimientos
    // mientras cada una mejore al menos 0.01 %.
    if (!opt.greedy || (budgeted && elapsed() > 0.9 * opt.timeLimit)) return;
    const int rg = std::max(1, (int)rlim);
    for (int pass = 0; pass < 50; ++pass) {
        const long long before = hp.total();
        long long accepted = 0;
        for (long long m = 0; m < M; ++m) {
            int c = movable[pickCell(rng)], x, y;
            if (!pickTarget(d, idx, c, rg, rng, x, y) || !hp.canMoveTo(c, x, y)) continue;
            if (hp.proposeMove(c, x, y) <= 0) {
                hp.commit();
                ++accepted;
            }
        }
        std::printf("  greedy %2d  HPWL=%11lld  R=%.4f  t=%8.1fs\n", pass, hp.total(), (double)accepted / M, elapsed());
        std::fflush(stdout);
        if (before - hp.total() < 1e-4 * before) break;
        if (budgeted && elapsed() > 0.9 * opt.timeLimit) break;
    }
    std::printf("Greedy terminado: HPWL=%lld, %.1f s\n", hp.total(), elapsed());
}

// ===========================================================================
// Prueba del HPWL incremental
// ===========================================================================
bool selfTest(Design& d, Hpwl& hp, std::mt19937_64& rng, long long moves) {
    std::vector<int> movable;
    for (int c = 0; c < d.numCells; ++c)
        if (!d.cellFixed[c]) movable.push_back(c);
    if (movable.empty()) return true;

    std::uniform_int_distribution<size_t> pickCell(0, movable.size() - 1);
    long long checkEvery = std::max(1LL, moves / 10), accepted = 0;
    bool ok = true;
    for (long long i = 1; i <= moves; ++i) {
        int c = movable[pickCell(rng)];
        const auto& sites = d.sitesOfType[d.cellType[c]];
        int s = sites[std::uniform_int_distribution<size_t>(0, sites.size() - 1)(rng)];
        int x = d.siteX(s), y = d.siteY(s);
        if (!hp.canMoveTo(c, x, y)) continue;
        long long before = hp.total();
        long long delta = hp.proposeMove(c, x, y);
        if (rng() & 1) {  // acepta la mitad, para probar también el camino "rechazar"
            hp.commit();
            ++accepted;
            if (hp.total() != before + delta) ok = false;
        }
        if (i % checkEvery == 0 || i == moves) {
            long long real = hp.computeFromScratch();
            std::printf("  [selftest] %lld movimientos (%lld aceptados): incremental=%lld recalculado=%lld %s\n", i,
                        accepted, hp.total(), real, real == hp.total() ? "OK" : "ERROR");
            if (real != hp.total()) ok = false;
        }
    }
    std::string legal = d.checkLegal();
    std::printf("  [selftest] legalidad: %s\n", legal.empty() ? "OK" : legal.c_str());
    std::printf("  [selftest] pines recorridos en re-escaneos: %lld\n", hp.fullScans());
    return ok && legal.empty();
}
