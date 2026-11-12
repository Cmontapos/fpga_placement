// design.cpp
// Lectura y escritura de archivos, construcción de índices y chequeo de legalidad.
#include "design.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {

// Lee el archivo completo en memoria. Es mucho más rápido que leer con
// std::getline + istringstream para archivos de varios MB (FPGA04.nets ~12 MB).
std::string readWholeFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("No se pudo abrir " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Divide una línea en tokens separados por espacios o tabs.
// Los string_view apuntan dentro del buffer del archivo (sin copias).
void splitTokens(const char* b, const char* e, std::vector<std::string_view>& out) {
    out.clear();
    while (b < e) {
        while (b < e && (*b == ' ' || *b == '\t' || *b == '\r')) ++b;
        const char* s = b;
        while (b < e && *b != ' ' && *b != '\t' && *b != '\r') ++b;
        if (b > s) out.emplace_back(s, b - s);
    }
}

// Llama fn(tokens, numeroDeLinea) para cada línea no vacía del archivo.
template <class F>
void forEachLine(const std::string& buf, F fn) {
    std::vector<std::string_view> tok;
    const char* p = buf.data();
    const char* end = p + buf.size();
    int lineNo = 0;
    while (p < end) {
        const char* nl = static_cast<const char*>(memchr(p, '\n', end - p));
        if (!nl) nl = end;
        ++lineNo;
        splitTokens(p, nl, tok);
        if (!tok.empty()) fn(tok, lineNo);
        p = nl + 1;
    }
}

int toInt(std::string_view s, const std::string& ctx) {
    int v = 0;
    bool neg = false;
    size_t i = 0;
    if (!s.empty() && s[0] == '-') { neg = true; i = 1; }
    if (i >= s.size()) throw std::runtime_error("Entero inválido en " + ctx);
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') throw std::runtime_error("Entero inválido '" + std::string(s) + "' en " + ctx);
        v = v * 10 + (s[i] - '0');
    }
    return neg ? -v : v;
}

std::unordered_map<std::string, int> g_cellIndex;  // nombre -> id, solo se usa al leer

}  // namespace

SiteType parseSiteType(const std::string& s) {
    if (s == "CLB") return CLB;
    if (s == "RAM") return RAM;
    if (s == "DSP") return DSP;
    if (s == "IO") return IO;
    if (s == "INVALID") return INVALID;
    throw std::runtime_error("Tipo de sitio desconocido: " + s);
}

const char* siteTypeName(int t) {
    static const char* names[] = {"CLB", "RAM", "DSP", "IO", "INVALID"};
    return (t >= 0 && t < NUM_SITE_TYPES) ? names[t] : "?";
}

// ---------------------------------------------------------------------------
// .sitemap:  primera línea "<ancho> <alto>", luego "<x> <y> <tipo>"
// ---------------------------------------------------------------------------
void Design::readSitemap(const std::string& path) {
    std::string buf = readWholeFile(path);
    bool first = true;
    forEachLine(buf, [&](const std::vector<std::string_view>& t, int ln) {
        std::string ctx = path + ":" + std::to_string(ln);
        if (first) {
            if (t.size() < 2) throw std::runtime_error("Encabezado inválido en " + ctx);
            W = toInt(t[0], ctx);
            H = toInt(t[1], ctx);
            siteType.assign(size_t(W) * H, INVALID);  // sitios no listados = INVALID
            first = false;
            return;
        }
        if (t.size() < 3) throw std::runtime_error("Línea inválida en " + ctx);
        int x = toInt(t[0], ctx), y = toInt(t[1], ctx);
        if (!inBounds(x, y)) throw std::runtime_error("Sitio fuera de la rejilla en " + ctx);
        siteType[site(x, y)] = parseSiteType(std::string(t[2]));
    });
    occupant.assign(size_t(W) * H, -1);
    sitesOfType.assign(NUM_SITE_TYPES, {});
    for (int s = 0; s < W * H; ++s) sitesOfType[siteType[s]].push_back(s);
}

// ---------------------------------------------------------------------------
// .pl:  "<nombre> <tipo> <x> <y> <M|F>"
// ---------------------------------------------------------------------------
void Design::readPl(const std::string& path) {
    if (W == 0) throw std::runtime_error("Hay que leer el .sitemap antes del .pl");
    std::string buf = readWholeFile(path);
    g_cellIndex.clear();
    forEachLine(buf, [&](const std::vector<std::string_view>& t, int ln) {
        std::string ctx = path + ":" + std::to_string(ln);
        if (t.size() < 5) throw std::runtime_error("Línea inválida en " + ctx);
        std::string name(t[0]);
        if (g_cellIndex.count(name)) throw std::runtime_error("Celda repetida " + name + " en " + ctx);
        int id = numCells++;
        g_cellIndex.emplace(name, id);
        cellName.push_back(name);
        cellType.push_back(parseSiteType(std::string(t[1])));
        int x = toInt(t[2], ctx), y = toInt(t[3], ctx);
        cellX.push_back(x);
        cellY.push_back(y);
        cellFixed.push_back(t[4] == "F" ? 1 : 0);
    });
    origX = cellX;
    origY = cellY;
    // Marca la ocupación inicial (el .pl original ya es legal).
    for (int c = 0; c < numCells; ++c) {
        if (!inBounds(cellX[c], cellY[c])) throw std::runtime_error("Celda fuera de la rejilla: " + cellName[c]);
        int s = site(cellX[c], cellY[c]);
        if (occupant[s] != -1) throw std::runtime_error("Traslape en el .pl original: " + cellName[c]);
        occupant[s] = c;
    }
}

// ---------------------------------------------------------------------------
// .nets:  "<nombre_net> <celda_1> ... <celda_n>"
// ---------------------------------------------------------------------------
void Design::readNets(const std::string& path) {
    if (numCells == 0) throw std::runtime_error("Hay que leer el .pl antes del .nets");
    std::string buf = readWholeFile(path);
    netStart.assign(1, 0);
    std::vector<int> pins;
    forEachLine(buf, [&](const std::vector<std::string_view>& t, int ln) {
        pins.clear();
        for (size_t i = 1; i < t.size(); ++i) {
            auto it = g_cellIndex.find(std::string(t[i]));
            if (it == g_cellIndex.end())
                throw std::runtime_error("Celda desconocida " + std::string(t[i]) + " en " + path + ":" + std::to_string(ln));
            pins.push_back(it->second);
        }
        // Elimina celdas repetidas dentro de una misma net (no cambian el HPWL
        // y romperían el conteo de bordes del bounding box incremental).
        std::sort(pins.begin(), pins.end());
        pins.erase(std::unique(pins.begin(), pins.end()), pins.end());
        netName.emplace_back(t[0]);
        netPins.insert(netPins.end(), pins.begin(), pins.end());
        netStart.push_back(int(netPins.size()));
        ++numNets;
    });

    // Construye celda -> nets (CSR) con conteo + prefijos.
    cellNetStart.assign(numCells + 1, 0);
    for (int c : netPins) cellNetStart[c + 1]++;
    for (int c = 0; c < numCells; ++c) cellNetStart[c + 1] += cellNetStart[c];
    cellNets.resize(netPins.size());
    std::vector<int> fill(cellNetStart.begin(), cellNetStart.end() - 1);
    for (int n = 0; n < numNets; ++n)
        for (int i = netStart[n]; i < netStart[n + 1]; ++i) cellNets[fill[netPins[i]]++] = n;

    g_cellIndex.clear();  // ya no se necesita
}

// ---------------------------------------------------------------------------
void Design::writePl(const std::string& path) const {
    std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) throw std::runtime_error("No se pudo escribir " + path);
    for (int c = 0; c < numCells; ++c)
        std::fprintf(f, "%s %s %d %d %c\n", cellName[c].c_str(), siteTypeName(cellType[c]), cellX[c], cellY[c],
                     cellFixed[c] ? 'F' : 'M');
    std::fclose(f);
}

// ---------------------------------------------------------------------------
std::string Design::checkLegal() const {
    std::vector<int> seen(size_t(W) * H, -1);
    for (int c = 0; c < numCells; ++c) {
        int x = cellX[c], y = cellY[c];
        if (!inBounds(x, y)) return cellName[c] + " fuera de la rejilla";
        int s = site(x, y);
        if (siteType[s] == INVALID) return cellName[c] + " en sitio INVALID";
        if (siteType[s] != cellType[c]) return cellName[c] + " en sitio de tipo incorrecto";
        if (seen[s] != -1) return cellName[c] + " traslapa con " + cellName[seen[s]];
        if (cellFixed[c] && (x != origX[c] || y != origY[c])) return cellName[c] + " es fija y fue movida";
        if (occupant[s] != c) return "tabla de ocupación inconsistente en " + cellName[c];
        seen[s] = c;
    }
    return "";
}

void Design::printStats() const {
    std::printf("Rejilla %dx%d | celdas %d | nets %d | pines %zu\n", W, H, numCells, numNets, netPins.size());
    for (int t = 0; t < INVALID; ++t) {
        int cells = 0, fixed = 0;
        for (int c = 0; c < numCells; ++c)
            if (cellType[c] == t) { ++cells; fixed += cellFixed[c]; }
        if (cells == 0) continue;
        std::printf("  %-4s celdas %6d (fijas %d) / sitios %6zu  -> utilización %5.1f%%\n", siteTypeName(t), cells,
                    fixed, sitesOfType[t].size(), 100.0 * cells / sitesOfType[t].size());
    }
    int maxDeg = 0;
    for (int n = 0; n < numNets; ++n) maxDeg = std::max(maxDeg, netDegree(n));
    std::printf("  grado máximo de net: %d\n", maxDeg);
}
