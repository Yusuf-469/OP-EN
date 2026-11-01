"""Report figures from the CSV files the C++ CLI writes to results/.

Plotting only: every number comes from results/*.csv (produced by uew_cli);
nothing is computed here. Writes SVG (vector, for the PDF report) and PNG.
Usage: python scripts/make_figures.py
"""
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "results"
OUT = RES / "plots"
COLORS = ["#c4421a", "#2e8540", "#7b3fa0", "#a06d00"]
plt.rcParams.update({"font.size": 9, "axes.grid": True, "grid.color": "#dde3ea", "svg.fonttype": "none",
                     "figure.dpi": 150, "axes.spines.top": False, "axes.spines.right": False})


def rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def by_mesh(path, x_key, y_key):
    groups = defaultdict(lambda: ([], []))
    for r in rows(path):
        groups[int(r["N"])][0].append(float(r[x_key]))
        groups[int(r["N"])][1].append(float(r[y_key]))
    return dict(sorted(groups.items()))


def save(fig, name):
    OUT.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(OUT / f"{name}.svg")
    fig.savefig(OUT / f"{name}.png")
    plt.close(fig)


def thermal():
    nodes = rows(RES / "thermal/thermal_nodes.csv")
    profile = by_mesh(RES / "thermal/thermal_error_profile.csv", "x_m", "T_fe_minus_exact_C")
    fig, (a, b) = plt.subplots(1, 2, figsize=(7.2, 2.7))
    n80 = [r for r in nodes if r["N"] == "80"]
    a.plot([float(r["x_m"]) for r in n80], [float(r["T_exact_C"]) for r in n80], "k-", lw=2, label="exact")
    for color, (n, _) in zip(COLORS, profile.items()):
        sel = [r for r in nodes if int(r["N"]) == n]
        a.plot([float(r["x_m"]) for r in sel], [float(r["T_fe_C"]) for r in sel], "o--", ms=2.2, lw=0.9,
               color=color, label=f"FE N={n}")
    a.set(xlabel="x (m)", ylabel="T (°C)", title="(a) Temperature: FE nodes vs exact")
    a.legend(fontsize=7)
    for color, (n, (x, e)) in zip(COLORS, profile.items()):
        b.plot(x, e, color=color, lw=0.9, label=f"N={n}")
    b.set(xlabel="x (m)", ylabel="T_FE − T_exact (°C)", title="(b) Pointwise error (0 at nodes)")
    b.legend(fontsize=7)
    save(fig, "thermal_solution")

    conv = rows(RES / "thermal/thermal_convergence.csv")
    h = [float(r["h_m"]) for r in conv]
    mid = [float(r["max_midpoint_error_C"]) for r in conv]
    nod = [float(r["max_nodal_error_C"]) for r in conv]
    fig, ax = plt.subplots(figsize=(3.6, 2.7))
    ax.loglog(h, mid, "o-", color="#0b5cad", label="max midpoint error")
    ax.loglog(h, [mid[0] * (v / h[0]) ** 2 for v in h], "--", color="#8796a5", label="slope 2 reference")
    ax.loglog(h, nod, "s:", color="#c4421a", ms=3, label="max nodal error (round-off)")
    ax.set(xlabel="element size h (m)", ylabel="|error| (°C)", title="Mesh convergence")
    ax.legend(fontsize=7)
    save(fig, "thermal_convergence")


def performance():
    summary = rows(RES / "performance/benchmark_summary.csv")
    samples = rows(RES / "performance/benchmark_samples.csv")
    pool = [r for r in summary if r["variant"] == "pool"]
    p = [int(r["workers"]) for r in pool]
    fig, (a, b) = plt.subplots(1, 2, figsize=(7.2, 2.6))
    a.plot(p, [float(r["speedup_vs_serial"]) for r in pool], "o-", color="#0b5cad", label="measured")
    a.plot(p, p, "--", color="#8796a5", label="ideal = p")
    a.set(xlabel="workers p", ylabel="speed-up T_serial/T_p (–)", title="(a) Speed-up", xticks=p)
    a.legend(fontsize=7)
    labels = ["serial"] + [f"p={w}" for w in p]
    groups = [[float(s["wall_ms"]) for s in samples if s["variant"] == "serial"]] + \
             [[float(s["wall_ms"]) for s in samples if s["variant"] == "pool" and int(s["workers"]) == w] for w in p]
    for i, g in enumerate(groups):
        b.plot([i] * len(g), g, "o", ms=3, alpha=0.6, color="#0b5cad")
    b.plot(range(len(groups)), [sorted(g)[len(g) // 2] for g in groups], "_", ms=18, mew=2, color="#c4421a",
           label="median")
    b.set(xticks=range(len(labels)), xticklabels=labels, ylabel="wall time (ms)", title="(b) 5 timed runs each")
    b.legend(fontsize=7)
    save(fig, "benchmark")


def backprop():
    loss = rows(RES / "backprop/backprop_loss.csv")
    fig, ax = plt.subplots(figsize=(3.6, 2.5))
    ax.semilogy([int(r["step"]) for r in loss], [float(r["loss"]) for r in loss], color="#0b5cad")
    ax.set(xlabel="gradient-descent step", ylabel="loss J (–)", title="Training loss, lr = 0.1")
    save(fig, "backprop_loss")


if __name__ == "__main__":
    thermal()
    performance()
    backprop()
    print("figures written to", OUT)
