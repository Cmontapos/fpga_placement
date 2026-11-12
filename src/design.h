// design.h
// Representación del problema: rejilla de la FPGA, celdas y nets.
//
// Decisiones principales:
//  - Todos los nombres (cell_123, net_45) se convierten a enteros al leer.
//    Dentro de la optimización nunca se usan strings.
//  - Las nets se guardan en formato CSR (Compressed Sparse Row): un arreglo
//    plano con todas las celdas de todas las nets y un arreglo de índices de
//    inicio. Es más compacto y rápido de recorrer que vector<vector<int>>.
//  - También se guarda la relación inversa celda -> nets (también CSR), que es
//    lo que se necesita para evaluar un movimiento: al mover una celda solo
//    cambian sus nets.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum SiteType : uint8_t { CLB = 0, RAM = 1, DSP = 2, IO = 3, INVALID = 4, NUM_SITE_TYPES = 5 };

SiteType parseSiteType(const std::string& s);
const char* siteTypeName(int t);

struct Design {
    // ---- Rejilla -------------------------------------------------------
    int W = 0, H = 0;                   // ancho y alto
    std::vector<uint8_t> siteType;      // tipo de cada sitio, índice = site(x, y)
    std::vector<int> occupant;          // celda en cada sitio, -1 si está libre
    std::vector<std::vector<int>> sitesOfType;  // índices de sitio por tipo

    int site(int x, int y) const { return x * H + y; }
    int siteX(int s) const { return s / H; }
    int siteY(int s) const { return s % H; }
    bool inBounds(int x, int y) const { return x >= 0 && x < W && y >= 0 && y < H; }

    // ---- Celdas (en el orden del .pl original) -------------------------
    int numCells = 0;
    std::vector<std::string> cellName;
    std::vector<uint8_t> cellType;
    std::vector<int> cellX, cellY;
    std::vector<uint8_t> cellFixed;     // 1 = F, 0 = M
    std::vector<int> origX, origY;      // posición del .pl original

    // ---- Nets (CSR) ----------------------------------------------------
    int numNets = 0;
    std::vector<std::string> netName;
    std::vector<int> netStart;          // celdas de la net n: netPins[netStart[n] .. netStart[n+1])
    std::vector<int> netPins;

    // ---- Relación inversa celda -> nets (CSR) --------------------------
    std::vector<int> cellNetStart;      // nets de la celda c: cellNets[cellNetStart[c] .. cellNetStart[c+1])
    std::vector<int> cellNets;

    int netDegree(int n) const { return netStart[n + 1] - netStart[n]; }
    int cellDegree(int c) const { return cellNetStart[c + 1] - cellNetStart[c]; }

    // ---- Entrada / salida ----------------------------------------------
    // Leen los archivos y lanzan std::runtime_error si algo no cuadra.
    // Orden obligatorio: readSitemap, readPl, readNets.
    void readSitemap(const std::string& path);
    void readPl(const std::string& path);
    void readNets(const std::string& path);
    void writePl(const std::string& path) const;

    // Verificación propia de legalidad (las mismas reglas que el evaluador).
    // Devuelve "" si es legal, o una descripción del primer problema.
    std::string checkLegal() const;

    void printStats() const;
};
