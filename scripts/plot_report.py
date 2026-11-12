#!/usr/bin/env python3
"""Gráficos de resultados para el informe (README) y la presentación.

Uso:  python3 scripts/plot_report.py        (escribe en figuras/)

  figuras/ablacion.png    aporte de cada pieza (results/ablation_fase2.csv)
  figuras/inner.png       calidad contra tiempo según inner_num (results/inner_fase2.csv)
  figuras/evolucion.png   HPWL de cada versión como % de la meta, por benchmark
"""
import csv
import os
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# Paleta (validada, modo claro): series en orden fijo; texto en tinta, no en color de serie
SERIE = ["#2a78d6", "#eb6834", "#1baf7a"]
GRIS = "#b4b2a9"
TINTA, TINTA2, EJE = "#0b0b0b", "#52514e", "#d9d8d2"
FONDO = "#fcfcfb"

plt.rcParams.update({
    "font.size": 13, "axes.edgecolor": EJE, "axes.labelcolor": TINTA2, "xtick.color": TINTA2,
    "ytick.color": TINTA2, "axes.titlecolor": TINTA, "axes.titlesize": 15, "figure.facecolor": FONDO,
    "axes.facecolor": FONDO, "axes.spines.top": False, "axes.spines.right": False,
})

META = {"FPGA00": 52880, "FPGA01": 1453168, "FPGA02": 2716304, "FPGA03": 11528160, "FPGA04": 21846668}


def fmt(n):
    return f"{n:,.0f}".replace(",", " ")


def ablacion():
    prom = defaultdict(list)
    with open("results/ablation_fase2.csv") as f:
        for r in csv.DictReader(f):
            prom[(r["benchmark"], r["config"])].append(int(r["hpwl"]))
    configs = [("base", "Fase 1 (α fijo, sin ventana)"), ("alfa", "Solo α adaptativo"),
               ("ventana", "Solo ventana"), ("ambas", "Ventana + α adaptativo")]
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.2), dpi=160, sharey=True)
    for ax, b in zip(axes, ("FPGA01", "FPGA02")):
        base = sum(prom[(b, "base")]) / len(prom[(b, "base")])
        vals = [100 * (sum(prom[(b, c)]) / len(prom[(b, c)])) / base for c, _ in configs]
        colores = [GRIS, GRIS, GRIS, SERIE[0]]
        y = range(len(configs))[::-1]
        ax.barh(list(y), vals, color=colores, height=0.6)
        for yi, v in zip(y, vals):
            ax.text(v + 2, yi, f"{v:.0f} %", va="center", color=TINTA, fontsize=13)
        ax.axvline(100, color=TINTA2, lw=1, ls=":")
        ax.set_yticks(list(y))
        ax.set_yticklabels([n for _, n in configs])
        ax.set_xlim(0, 165)
        ax.set_xlabel("HPWL promedio (3 semillas), % de la fase 1")
        ax.set_title(b, loc="left")
        ax.tick_params(axis="y", length=0)
    fig.tight_layout()
    fig.savefig("figuras/ablacion.png")


def inner():
    filas = []
    with open("results/inner_fase2.csv") as f:
        for r in csv.DictReader(f):
            filas.append((float(r["config"].replace("inner", "")), int(r["hpwl"])))
    filas.sort()
    # Tiempo real aproximado de una corrida sola: 45 s con inner = 1, lineal en inner
    xs = [45 * k for k, _ in filas]
    ys = [h / 1000 for _, h in filas]
    fig, ax = plt.subplots(figsize=(8, 4.5), dpi=160)
    ax.plot(xs, ys, color=SERIE[0], lw=2, marker="o", ms=9, markeredgecolor=FONDO, markeredgewidth=2)
    for i, ((k, h), x) in enumerate(zip(filas, xs)):
        ultimo = i == len(filas) - 1
        ax.annotate(f"inner {k:g}\n{fmt(h)}", (x, h / 1000), textcoords="offset points",
                    xytext=(-14, 4) if ultimo else (0, 12), ha="right" if ultimo else "center",
                    va="bottom", color=TINTA, fontsize=11)
    ax.axvline(360, color=TINTA2, lw=1, ls=":")
    ax.text(355, max(ys) * 1.06, "límite FPGA01\n(6 min)", ha="right", va="top", color=TINTA2, fontsize=11)
    ax.set_xlabel("Tiempo de ejecución aproximado (s)")
    ax.set_ylabel("HPWL (miles)")
    ax.set_title("FPGA01: más movimientos por temperatura, mejor HPWL", loc="left")
    ax.set_ylim(min(ys) * 0.9, max(ys) * 1.12)
    ax.set_xlim(0, 400)
    ax.grid(axis="y", color=EJE, lw=0.6)
    fig.tight_layout()
    fig.savefig("figuras/inner.png")


def evolucion():
    datos = {  # mejor resultado de cada versión (semilla 1 o mejor de 5)
        "FPGA00": (25809, 19284, 11434), "FPGA01": (740738, 442116, 368386),
        "FPGA02": (1198944, 773713, 699386), "FPGA03": (3987586, 3081084, 2870794),
        "FPGA04": (6477992, 5483726, 5308558),
    }
    nombres = ["Fase 1: annealing básico", "Fase 2: ventana + α adaptativo", "Final: + presupuesto de tiempo"]
    fig, ax = plt.subplots(figsize=(12, 4.8), dpi=160)
    w = 0.26
    bs = list(datos)
    for i, nombre in enumerate(nombres):
        xs = [j + (i - 1) * (w + 0.02) for j in range(len(bs))]
        vals = [100 * datos[b][i] / META[b] for b in bs]
        ax.bar(xs, vals, width=w, color=SERIE[i], label=nombre)
        if i == 2:
            for x, v in zip(xs, vals):
                ax.text(x, v + 1.5, f"{v:.0f} %", ha="center", color=TINTA, fontsize=11)
    ax.axhline(100, color=TINTA2, lw=1, ls=":")
    ax.text(-0.45, 101.5, "meta = 100 %", ha="left", va="bottom", color=TINTA2, fontsize=11)
    ax.set_xticks(range(len(bs)))
    ax.set_xticklabels(bs)
    ax.set_ylabel("HPWL, % de la meta (menos es mejor)")
    ax.set_ylim(0, 110)
    ax.legend(frameon=False, loc="upper right", bbox_to_anchor=(1, 0.88), fontsize=11)
    ax.grid(axis="y", color=EJE, lw=0.6)
    ax.set_axisbelow(True)
    ax.tick_params(axis="x", length=0)
    fig.tight_layout()
    fig.savefig("figuras/evolucion.png")


if __name__ == "__main__":
    os.makedirs("figuras", exist_ok=True)
    ablacion()
    inner()
    evolucion()
    print("escritas figuras/ablacion.png, figuras/inner.png, figuras/evolucion.png")
