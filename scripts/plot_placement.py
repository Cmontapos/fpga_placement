#!/usr/bin/env python3
"""Imágenes del placement para el informe y la presentación.

Uso:
  scripts/plot_placement.py placement <sitemap> <pl_inicial> <pl_final> <salida.png> [--titulo T]
      Rejilla de la FPGA con cada celda en su sitio, coloreada por tipo:
      placement inicial (izquierda) y final (derecha).
  scripts/plot_placement.py zoom <sitemap> <pl_final> <salida.png> [--margen M] [--titulo T]
      Acercamiento a la zona ocupada del placement final.
  scripts/plot_placement.py curva <log> <salida.png> [--meta HPWL] [--titulo T]
      Curva de convergencia del annealing (HPWL y tasa de aceptación por temperatura).
"""
import argparse
import re

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

# Colores por tipo (fondo = sitios vacíos, punto = celda colocada)
COLORES = {"CLB": "#2a78d6", "RAM": "#eb6834", "DSP": "#1baf7a", "IO": "#8a6a00"}
FONDO = {"CLB": "#E6ECF5", "RAM": "#FBE5D3", "DSP": "#D9F0E4", "IO": "#F7D5C6", "INVALID": "#F4F3EF"}


def leer_sitemap(path):
    with open(path) as f:
        w, h = map(int, f.readline().split())
        grid = np.full((h, w), "INVALID", dtype=object)
        for line in f:
            p = line.split()
            if len(p) == 3:
                grid[int(p[1]), int(p[0])] = p[2]
    return w, h, grid


def leer_pl(path):
    celdas = {}
    with open(path) as f:
        for line in f:
            p = line.split()
            if len(p) >= 4:
                celdas.setdefault(p[1], []).append((int(p[2]), int(p[3])))
    return {t: np.array(v) for t, v in celdas.items()}


def dibujar_rejilla(ax, w, h, grid):
    tipos = list(FONDO)
    idx = np.vectorize(tipos.index)(grid)
    cmap = matplotlib.colors.ListedColormap([FONDO[t] for t in tipos])
    ax.imshow(idx, cmap=cmap, vmin=0, vmax=len(tipos) - 1, origin="lower", interpolation="nearest",
              extent=(-0.5, w - 0.5, -0.5, h - 0.5))


def placement(a):
    w, h, grid = leer_sitemap(a.sitemap)
    fig, axes = plt.subplots(1, 2, figsize=(10, 13), dpi=160)
    for ax, path, nombre in ((axes[0], a.inicial, "Inicial (aleatorio)"), (axes[1], a.final, "Final")):
        dibujar_rejilla(ax, w, h, grid)
        celdas = leer_pl(path)
        for t in ("CLB", "RAM", "DSP", "IO"):
            if t in celdas:
                xy = celdas[t]
                s = 1.2 if t == "CLB" else 6
                ax.scatter(xy[:, 0], xy[:, 1], s=s, c=COLORES[t], marker="s", linewidths=0, label=t)
        ax.set_title(nombre, fontsize=15)
        ax.set_xlim(-0.5, w - 0.5)
        ax.set_ylim(-0.5, h - 0.5)
        ax.set_aspect("equal")
        ax.set_xticks([0, w // 2, w - 1])
        ax.set_yticks([0, h // 2, h - 1])
    leg = axes[1].legend(loc="upper right", markerscale=4, fontsize=10, framealpha=0.9)
    for hnd in leg.legend_handles:
        hnd.set_sizes([30])
    if a.titulo:
        fig.suptitle(a.titulo, fontsize=17)
    fig.tight_layout()
    fig.savefig(a.salida, transparent=False, facecolor="white")
    print("escrito", a.salida)


# Metas de HPWL del enunciado (solo para dibujar la línea de referencia)
METAS = {"FPGA00": 52880, "FPGA01": 1453168, "FPGA02": 2716304, "FPGA03": 11528160,
         "FPGA04": 21846668, "FPGA05": 38891148}


def curva(a):
    if not a.meta:
        a.meta = METAS.get(a.titulo, 0)
    it, hpwl, acc = [], [], []
    pat = re.compile(r"it=\s*(\d+)\s+T=\s*[\d.]+\s+HPWL=\s*(\d+)\s+R=([\d.]+)")
    with open(a.log) as f:
        for line in f:
            m = pat.search(line)
            if m:
                it.append(int(m.group(1)))
                hpwl.append(int(m.group(2)))
                acc.append(float(m.group(3)))
    # Dos paneles con el mismo eje x (nunca dos escalas y en un mismo gráfico)
    fig, (ax, ax2) = plt.subplots(2, 1, figsize=(10, 6), dpi=160, sharex=True,
                                  gridspec_kw={"height_ratios": [2.2, 1]})
    ax.plot(it, np.array(hpwl) / 1e6, color="#2a78d6", lw=2)
    ax.set_ylabel("HPWL (millones)")
    if a.meta:
        ax.axhline(a.meta / 1e6, color="#52514e", ls=":", lw=1.2)
        ax.text(it[-1], a.meta / 1e6, "meta", ha="right", va="bottom", color="#52514e")
    ax2.plot(it, acc, color="#2a78d6", lw=2)
    ax2.axhline(0.44, color="#52514e", ls=":", lw=1.2)
    ax2.text(it[-1], 0.46, "objetivo 0,44", ha="right", va="bottom", color="#52514e")
    ax2.set_ylabel("Aceptación R")
    ax2.set_ylim(0, 1.05)
    ax2.set_xlabel("Temperatura (iteración)")
    for x in (ax, ax2):
        x.spines["top"].set_visible(False)
        x.spines["right"].set_visible(False)
        x.grid(axis="y", color="#e4e3dd", lw=0.6)
    if a.titulo:
        ax.set_title(a.titulo, loc="left")
    fig.tight_layout()
    fig.savefig(a.salida, facecolor="white")
    print("escrito", a.salida)


def zoom(a):
    """Acercamiento a la zona ocupada del placement final (útil cuando el
    circuito es pequeño y en el chip completo casi no se ve)."""
    w, h, grid = leer_sitemap(a.sitemap)
    celdas = leer_pl(a.final)
    movibles = np.vstack([v for t, v in celdas.items() if t != "IO"])
    x0, y0 = movibles.min(axis=0) - a.margen
    x1, y1 = movibles.max(axis=0) + a.margen
    x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, w - 1), min(y1, h - 1)
    fig, ax = plt.subplots(figsize=(10, 10 * (y1 - y0 + 1) / (x1 - x0 + 1)), dpi=160)
    dibujar_rejilla(ax, w, h, grid)
    for t in ("CLB", "RAM", "DSP", "IO"):
        if t in celdas:
            xy = celdas[t]
            ax.scatter(xy[:, 0], xy[:, 1], s=40, c=COLORES[t], marker="s", linewidths=0, label=t)
    ax.set_xlim(x0 - 0.5, x1 + 0.5)
    ax.set_ylim(y0 - 0.5, y1 + 0.5)
    ax.set_aspect("equal")
    ax.legend(loc="upper right", fontsize=11, framealpha=0.9)
    if a.titulo:
        ax.set_title(a.titulo, fontsize=15, loc="left")
    fig.tight_layout()
    fig.savefig(a.salida, facecolor="white")
    print("escrito", a.salida)


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    q = sub.add_parser("placement")
    q.add_argument("sitemap")
    q.add_argument("inicial")
    q.add_argument("final")
    q.add_argument("salida")
    q.add_argument("--titulo", default="")
    q.set_defaults(fn=placement)
    z = sub.add_parser("zoom")
    z.add_argument("sitemap")
    z.add_argument("final")
    z.add_argument("salida")
    z.add_argument("--margen", type=int, default=4)
    z.add_argument("--titulo", default="")
    z.set_defaults(fn=zoom)
    c = sub.add_parser("curva")
    c.add_argument("log")
    c.add_argument("salida")
    c.add_argument("--meta", type=float, default=0)
    c.add_argument("--titulo", default="")
    c.set_defaults(fn=curva)
    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
