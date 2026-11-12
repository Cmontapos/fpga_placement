// main.cpp
// Uso:
//   ./placer <archivo.sitemap> <archivo.nets> <archivo.pl> <salida.pl> [--seed N] [--selftest N]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

#include "design.h"
#include "hpwl.h"
#include "placer.h"

namespace {

using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

void usage(const char* prog) {
    std::fprintf(stderr,
                 "Uso: %s <sitemap> <nets> <pl> <salida.pl> [--seed N] [--selftest N] [--inner X] [--alpha A]\n"
                 "        [--no-window] [--no-greedy] [--time-limit S]\n"
                 "  --seed N       semilla del generador aleatorio (default 1)\n"
                 "  --selftest N   prueba el HPWL incremental con N movimientos aleatorios\n"
                 "  --inner X      movimientos por temperatura = X * N^(4/3) (default 1)\n"
                 "  --time-limit S tiempo máximo del benchmark (s): calibra M para usar ~50%% de él,\n"
                 "                 con freno de seguridad al 80%%\n"
                 "  --alpha A      usa un alfa FIJO en lugar del adaptativo (experimentos)\n"
                 "  --no-window    sin ventana: destinos en cualquier parte del chip (experimentos)\n"
                 "  --no-greedy    sin el refinamiento greedy final (experimentos)\n",
                 prog);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 5) {
        usage(argv[0]);
        return 1;
    }
    const std::string sitemapPath = argv[1], netsPath = argv[2], plPath = argv[3], outPath = argv[4];
    Options opt;
    for (int i = 5; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) opt.seed = std::strtoull(argv[++i], nullptr, 10);
        else if (!std::strcmp(argv[i], "--selftest") && i + 1 < argc) opt.selftest = std::atoll(argv[++i]);
        else if (!std::strcmp(argv[i], "--inner") && i + 1 < argc) opt.innerNum = std::atof(argv[++i]);
        else if (!std::strcmp(argv[i], "--alpha") && i + 1 < argc) {
            opt.alpha = std::atof(argv[++i]);
            opt.adaptiveAlpha = false;
        }
        else if (!std::strcmp(argv[i], "--no-window")) opt.useWindow = false;
        else if (!std::strcmp(argv[i], "--no-greedy")) opt.greedy = false;
        else if (!std::strcmp(argv[i], "--time-limit") && i + 1 < argc) opt.timeLimit = std::atof(argv[++i]);
        else {
            usage(argv[0]);
            return 1;
        }
    }

    const auto t0 = Clock::now();
    try {
        Design d;
        d.readSitemap(sitemapPath);
        d.readPl(plPath);
        d.readNets(netsPath);
        d.printStats();

        Hpwl hp(d);
        const long long initial = hp.total();
        std::printf("Lectura: %.2f s | HPWL inicial: %lld\n", secondsSince(t0), initial);

        std::mt19937_64 rng(opt.seed);
        const auto tAlg = Clock::now();
        if (opt.selftest > 0) {
            bool ok = selfTest(d, hp, rng, opt.selftest);
            std::printf("Selftest: %s\n", ok ? "PASS" : "FAIL");
            if (!ok) return 2;
        } else {
            runPlacement(d, hp, rng, opt);
        }
        const double algSeconds = secondsSince(tAlg);

        // Verificación final antes de escribir
        std::string err = d.checkLegal();
        if (!err.empty()) {
            std::fprintf(stderr, "ERROR: placement ilegal: %s\n", err.c_str());
            return 3;
        }
        const long long final = hp.computeFromScratch();
        if (final != hp.total())
            std::fprintf(stderr, "AVISO: HPWL incremental (%lld) != recalculado (%lld)\n", hp.total(), final);

        d.writePl(outPath);
        std::printf("Algoritmo: %.2f s | HPWL final: %lld (%.2f%% vs inicial)\n", algSeconds, final,
                    100.0 * (final - initial) / initial);
        std::printf("Tiempo total: %.2f s | salida: %s\n", secondsSince(t0), outPath.c_str());
        // Línea fácil de procesar desde scripts:
        std::printf("RESULT hpwl=%lld time=%.3f seed=%llu\n", final, secondsSince(t0),
                    (unsigned long long)opt.seed);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ERROR: %s\n", e.what());
        return 1;
    }
    return 0;
}
