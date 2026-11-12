// hpwl.h
// Cálculo incremental del HPWL.
//
// Idea: cada net guarda su bounding box (xmin, xmax, ymin, ymax) y además
// CUÁNTAS celdas están exactamente sobre cada borde. Con eso, al mover una
// celda casi siempre se puede actualizar la caja en O(1):
//   - si la celda sale de un borde donde había más de una celda, el borde no cambia;
//   - si era la única celda en ese borde, hay que recorrer la net para encontrar
//     el nuevo borde (caso poco frecuente).
// Esta técnica es la que usa VPR (Betz & Rose, 1997) y es importante aquí porque
// hay nets con más de 45 000 celdas (la del reloj): recorrerlas en cada
// movimiento haría el programa inútilmente lento.
//
// Uso típico (por ejemplo en simulated annealing):
//     long long d = hp.proposeMove(c, x, y);   // no cambia nada todavía
//     if (aceptar(d)) hp.commit();             // aplica el movimiento
//     // si no se acepta, simplemente no se llama commit()
#pragma once

#include <vector>

#include "design.h"

struct NetBox {
    int xmin, xmax, ymin, ymax;
    int nxmin, nxmax, nymin, nymax;  // cantidad de celdas sobre cada borde
    int hpwl() const { return (xmax - xmin) + (ymax - ymin); }
};

class Hpwl {
public:
    explicit Hpwl(Design& d);

    long long total() const { return total_; }
    int netHpwl(int n) const { return box_[n].hpwl(); }
    const NetBox& box(int n) const { return box_[n]; }

    // Recalcula todas las cajas desde cero (usar después de cambiar
    // posiciones "a mano", fuera de proposeMove/commit).
    void rebuild();

    // HPWL total calculado desde cero, sin tocar el estado. Sirve para verificar.
    long long computeFromScratch() const;

    // ¿Puede la celda c ir al sitio (x, y)? Revisa límites, tipo, que c sea
    // móvil y que el sitio no esté ocupado por una celda fija.
    bool canMoveTo(int c, int x, int y) const;

    // Propone mover la celda c al sitio (x, y). Si el sitio está ocupado por
    // otra celda móvil, la propuesta es un INTERCAMBIO. Devuelve el cambio de
    // HPWL (negativo = mejora). No modifica nada; requiere canMoveTo(c, x, y).
    long long proposeMove(int c, int x, int y);

    // Aplica la última propuesta.
    void commit();

    // Pines recorridos al reconstruir bordes (estadística de costo).
    long long fullScans() const { return fullScans_; }

private:
    void addNetDelta(int n, int mover, int nx, int ny);
    void scanX(int n, int mover, int nx, NetBox& b);
    void scanY(int n, int mover, int ny, NetBox& b);
    NetBox boxFromScratch(int n) const;

    Design& d_;
    std::vector<NetBox> box_;
    long long total_ = 0;

    // Estado de la última propuesta
    int pa_ = -1, pb_ = -1;            // celda movida y celda desplazada (-1 si el sitio estaba libre)
    int pax_, pay_, pbx_, pby_;        // posiciones nuevas
    long long pdelta_ = 0;
    std::vector<int> touchedNets_;     // nets afectadas
    std::vector<NetBox> touchedBox_;   // su nueva caja
    std::vector<int> markA_, markB_;   // marcas para detectar nets compartidas por a y b
    int stamp_ = 0;
    long long fullScans_ = 0;
};
