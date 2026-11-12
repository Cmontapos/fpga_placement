// hpwl.cpp
#include "hpwl.h"

#include <algorithm>
#include <climits>

namespace {

// Actualiza un eje (x o y) de la caja cuando una celda pasa de o a n.
// Devuelve false si la celda era la única sobre un borde que abandona:
// en ese caso el nuevo borde no se conoce y hay que recorrer la net.
inline bool update1D(int& mn, int& cmn, int& mx, int& cmx, int o, int n) {
    if (o == n) return true;
    // 1) Agregar la posición nueva
    if (n < mn) { mn = n; cmn = 1; }
    else if (n == mn) ++cmn;
    if (n > mx) { mx = n; cmx = 1; }
    else if (n == mx) ++cmx;
    // 2) Quitar la posición vieja
    if (o == mn && --cmn == 0) return false;
    if (o == mx && --cmx == 0) return false;
    return true;
}

}  // namespace

Hpwl::Hpwl(Design& d) : d_(d) {
    markA_.assign(d_.numNets, 0);
    markB_.assign(d_.numNets, 0);
    rebuild();
}

NetBox Hpwl::boxFromScratch(int n) const {
    NetBox b{INT_MAX, INT_MIN, INT_MAX, INT_MIN, 0, 0, 0, 0};
    if (d_.netDegree(n) == 0) return NetBox{0, 0, 0, 0, 0, 0, 0, 0};
    for (int i = d_.netStart[n]; i < d_.netStart[n + 1]; ++i) {
        int c = d_.netPins[i], x = d_.cellX[c], y = d_.cellY[c];
        if (x < b.xmin) { b.xmin = x; b.nxmin = 1; } else if (x == b.xmin) ++b.nxmin;
        if (x > b.xmax) { b.xmax = x; b.nxmax = 1; } else if (x == b.xmax) ++b.nxmax;
        if (y < b.ymin) { b.ymin = y; b.nymin = 1; } else if (y == b.ymin) ++b.nymin;
        if (y > b.ymax) { b.ymax = y; b.nymax = 1; } else if (y == b.ymax) ++b.nymax;
    }
    return b;
}

void Hpwl::rebuild() {
    box_.resize(d_.numNets);
    total_ = 0;
    for (int n = 0; n < d_.numNets; ++n) {
        box_[n] = boxFromScratch(n);
        total_ += box_[n].hpwl();
    }
}

long long Hpwl::computeFromScratch() const {
    long long t = 0;
    for (int n = 0; n < d_.numNets; ++n) t += boxFromScratch(n).hpwl();
    return t;
}

bool Hpwl::canMoveTo(int c, int x, int y) const {
    if (d_.cellFixed[c] || !d_.inBounds(x, y)) return false;
    int s = d_.site(x, y);
    if (d_.siteType[s] != d_.cellType[c]) return false;
    int o = d_.occupant[s];
    return o == -1 || !d_.cellFixed[o];
}

void Hpwl::scanX(int n, int mover, int nx, NetBox& b) {
    fullScans_ += d_.netDegree(n);
    b.xmin = INT_MAX; b.xmax = INT_MIN; b.nxmin = b.nxmax = 0;
    for (int i = d_.netStart[n]; i < d_.netStart[n + 1]; ++i) {
        int c = d_.netPins[i];
        int x = (c == mover) ? nx : d_.cellX[c];
        if (x < b.xmin) { b.xmin = x; b.nxmin = 1; } else if (x == b.xmin) ++b.nxmin;
        if (x > b.xmax) { b.xmax = x; b.nxmax = 1; } else if (x == b.xmax) ++b.nxmax;
    }
}

void Hpwl::scanY(int n, int mover, int ny, NetBox& b) {
    fullScans_ += d_.netDegree(n);
    b.ymin = INT_MAX; b.ymax = INT_MIN; b.nymin = b.nymax = 0;
    for (int i = d_.netStart[n]; i < d_.netStart[n + 1]; ++i) {
        int c = d_.netPins[i];
        int y = (c == mover) ? ny : d_.cellY[c];
        if (y < b.ymin) { b.ymin = y; b.nymin = 1; } else if (y == b.ymin) ++b.nymin;
        if (y > b.ymax) { b.ymax = y; b.nymax = 1; } else if (y == b.ymax) ++b.nymax;
    }
}

// Calcula la caja nueva de la net n si la celda "mover" pasa a (nx, ny),
// la guarda en la lista de nets tocadas y acumula el delta de HPWL.
void Hpwl::addNetDelta(int n, int mover, int nx, int ny) {
    const NetBox& old = box_[n];
    NetBox b = old;
    if (!update1D(b.xmin, b.nxmin, b.xmax, b.nxmax, d_.cellX[mover], nx)) scanX(n, mover, nx, b);
    if (!update1D(b.ymin, b.nymin, b.ymax, b.nymax, d_.cellY[mover], ny)) scanY(n, mover, ny, b);
    pdelta_ += b.hpwl() - old.hpwl();
    touchedNets_.push_back(n);
    touchedBox_.push_back(b);
}

long long Hpwl::proposeMove(int a, int x, int y) {
    touchedNets_.clear();
    touchedBox_.clear();
    pdelta_ = 0;
    const int ox = d_.cellX[a], oy = d_.cellY[a];
    const int b = d_.occupant[d_.site(x, y)];
    pa_ = a; pax_ = x; pay_ = y;
    pb_ = (b == a) ? -1 : b;
    pbx_ = ox; pby_ = oy;
    if (b == a) return 0;

    // Marca las nets de a y de b. Una net que contiene a ambas no cambia con
    // un intercambio (el conjunto de posiciones es el mismo), así que se omite.
    if (stamp_ == INT_MAX) {
        std::fill(markA_.begin(), markA_.end(), 0);
        std::fill(markB_.begin(), markB_.end(), 0);
        stamp_ = 0;
    }
    ++stamp_;
    for (int i = d_.cellNetStart[a]; i < d_.cellNetStart[a + 1]; ++i) markA_[d_.cellNets[i]] = stamp_;
    if (pb_ >= 0)
        for (int i = d_.cellNetStart[b]; i < d_.cellNetStart[b + 1]; ++i) markB_[d_.cellNets[i]] = stamp_;

    for (int i = d_.cellNetStart[a]; i < d_.cellNetStart[a + 1]; ++i) {
        int n = d_.cellNets[i];
        if (pb_ >= 0 && markB_[n] == stamp_) continue;
        addNetDelta(n, a, x, y);
    }
    if (pb_ >= 0) {
        for (int i = d_.cellNetStart[b]; i < d_.cellNetStart[b + 1]; ++i) {
            int n = d_.cellNets[i];
            if (markA_[n] == stamp_) continue;
            addNetDelta(n, b, ox, oy);
        }
    }
    return pdelta_;
}

void Hpwl::commit() {
    if (pa_ < 0) return;
    for (size_t i = 0; i < touchedNets_.size(); ++i) box_[touchedNets_[i]] = touchedBox_[i];
    total_ += pdelta_;

    const int oldSite = d_.site(d_.cellX[pa_], d_.cellY[pa_]);
    const int newSite = d_.site(pax_, pay_);
    d_.occupant[oldSite] = pb_;  // -1 si el destino estaba libre
    d_.occupant[newSite] = pa_;
    d_.cellX[pa_] = pax_;
    d_.cellY[pa_] = pay_;
    if (pb_ >= 0) {
        d_.cellX[pb_] = pbx_;
        d_.cellY[pb_] = pby_;
    }
    pa_ = pb_ = -1;
    touchedNets_.clear();
    touchedBox_.clear();
}
