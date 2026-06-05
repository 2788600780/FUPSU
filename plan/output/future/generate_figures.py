"""Generate publication-quality figures for F-PSU USENIX paper."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np
import os

# Style: clean, publication-ready
plt.rcParams.update({
    "font.family": "serif", "font.size": 9,
    "axes.labelsize": 10, "axes.titlesize": 11,
    "legend.fontsize": 8, "xtick.labelsize": 8, "ytick.labelsize": 8,
    "figure.dpi": 300, "savefig.bbox": "tight", "savefig.pad_inches": 0.05,
    "lines.linewidth": 1.2, "lines.markersize": 4,
})

outdir = os.path.dirname(os.path.abspath(__file__))
COLORS = ["#2166AC", "#B2182B", "#4DAF4A", "#FF7F00"]
GRAY = "#999999"

# ============================================================
# Figure 1: Communication Reduction
# ============================================================
def fig_communication():
    n_vals = [2**14, 2**16, 2**18, 2**20]
    labels = [r"$2^{14}$", r"$2^{16}$", r"$2^{18}$", r"$2^{20}$"]
    # Data from Table 2 (tab:comm), 0.1% update ratio
    full_kb   = [576, 2304, 9216, 36864]
    delta_kb  = [0.9, 3.6, 14.3, 57.3]
    reduction = [658, 648, 643, 643]

    x = np.arange(len(n_vals))
    width = 0.35

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(6.0, 2.2),
                                    gridspec_kw={"width_ratios": [1.0, 0.55]})

    # Left: log-scale bars
    bars_full = ax1.bar(x - width/2, full_kb, width, color=COLORS[1],
                         alpha=0.85, label="Static (full IBLT)")
    bars_delta = ax1.bar(x + width/2, delta_kb, width, color=COLORS[0],
                          alpha=0.85, label="F-PSU (delta IBLT)")
    ax1.set_yscale("log")
    ax1.set_ylabel("Per-epoch data exchange (KB)")
    ax1.set_xticks(x)
    ax1.set_xticklabels(labels)
    ax1.legend(fontsize=7, loc="upper left")
    ax1.grid(axis="y", alpha=0.3, lw=0.5)

    # Annotate reduction factors
    for i in range(len(n_vals)):
        ax1.annotate(r"$\times$" + str(reduction[i]),
                     xy=(x[i], delta_kb[i] * 1.5), ha="center",
                     fontsize=7, fontweight="bold", color=COLORS[0])

    # Right: reduction factor line
    ax2.plot(labels, reduction, "o-", color=COLORS[0], markersize=5, lw=1.5)
    ax2.set_ylabel("Communication reduction")
    ax2.axhline(y=643, color=GRAY, lw=0.6, ls="--")
    ax2.annotate(r"$\sim 643\times$", xy=(1.5, 648), fontsize=7, color=GRAY)
    ax2.grid(axis="y", alpha=0.3, lw=0.5)

    fig.tight_layout()
    fig.savefig(os.path.join(outdir, "fig_communication.pdf"))
    plt.close(fig)
    print("  fig_communication.pdf")


# ============================================================
# Figure 2: Network Profile Comparison
# ============================================================
def fig_network():
    n_sizes = [r"$2^{14}$", r"$2^{16}$", r"$2^{18}$", r"$2^{20}$"]
    data = {
        "LAN":       [20,   94,    742,   2271],
        "200 Mbps":  [84,   170,   615,   2492],
        "50 Mbps":   [162,  313,   999,   3401],
        "5 Mbps":    [400,  1233,  4663,  None],
    }
    x = np.arange(len(n_sizes))
    width = 0.2

    fig, ax = plt.subplots(figsize=(5.5, 2.2))
    for idx, (label, vals) in enumerate(data.items()):
        xs = [i for i, v in enumerate(vals) if v is not None]
        vs = [v for v in vals if v is not None]
        ax.bar([xi + (idx - 1.5) * width for xi in xs], vs, width,
               color=COLORS[idx], alpha=0.85, label=label)

    ax.set_yscale("log")
    ax.set_ylabel("Epoch time (ms)")
    ax.set_xticks(x)
    ax.set_xticklabels(n_sizes)
    ax.legend(fontsize=7, ncol=2)
    ax.grid(axis="y", alpha=0.3, lw=0.5)

    fig.tight_layout()
    fig.savefig(os.path.join(outdir, "fig_network.pdf"))
    plt.close(fig)
    print("  fig_network.pdf")


# ============================================================
# Figure 3: Delta Ratio Sweep (break-even analysis)
# ============================================================
def fig_delta_sweep():
    # Data from Table delta-sweep, n=2^16
    ratios = [0.1, 1, 5, 10, 25, 50]
    epoch_gt0 = [2.3, 9.1, 32.0, 62.5, 155, 312]
    epoch0 = 94  # constant

    fig, ax = plt.subplots(figsize=(3.2, 2.0))
    ax.plot(ratios, epoch_gt0, "o-", color=COLORS[0], label=r"Epoch $>$0 (delta)")
    ax.axhline(y=epoch0, color=COLORS[1], lw=1.0, ls="--", label="Epoch 0 (full)")

    # Break-even annotation
    ax.axvline(x=50, color=GRAY, lw=0.6, ls=":")
    ax.annotate("Break-even\n$\\approx 50\\%$", xy=(48, 250), fontsize=7,
                ha="right", color=GRAY)

    ax.set_xlabel(r"$|\Delta|/n$ (\%)")
    ax.set_ylabel("Epoch time (ms)")
    ax.set_xscale("log")
    ax.legend(fontsize=7)
    ax.grid(alpha=0.3, lw=0.5)

    fig.tight_layout()
    fig.savefig(os.path.join(outdir, "fig_delta_sweep.pdf"))
    plt.close(fig)
    print("  fig_delta_sweep.pdf")


# ============================================================
# Figure 4: Amortized Cell Operations
# ============================================================
def fig_amortized():
    n_vals = [2**14, 2**16, 2**18, 2**20]
    labels = [r"$2^{14}$", r"$2^{16}$", r"$2^{18}$", r"$2^{20}$"]
    # Data from Table 3 (tab:amortized), millions
    static = [13.1, 52.4, 210, 839]
    fpsu   = [0.115, 0.461, 1.84, 7.36]

    x = np.arange(len(n_vals))
    width = 0.35

    fig, ax = plt.subplots(figsize=(3.5, 2.0))
    ax.bar(x - width/2, static, width, color=COLORS[1], alpha=0.85,
           label="Static (full re-encode)")
    ax.bar(x + width/2, fpsu, width, color=COLORS[0], alpha=0.85,
           label="F-PSU (incremental)")

    # Annotate savings
    for i in range(len(n_vals)):
        ax.annotate(r"$\mathbf{114\times}$", xy=(x[i], fpsu[i] * 1.3),
                    ha="center", fontsize=7, fontweight="bold", color=COLORS[0])

    ax.set_ylabel("Total IBLT cell ops (millions)")
    ax.set_yscale("log")
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend(fontsize=7)
    ax.grid(axis="y", alpha=0.3, lw=0.5)

    fig.tight_layout()
    fig.savefig(os.path.join(outdir, "fig_amortized.pdf"))
    plt.close(fig)
    print("  fig_amortized.pdf")


# ============================================================
if __name__ == "__main__":
    print("Generating figures...")
    fig_communication()
    fig_network()
    fig_delta_sweep()
    fig_amortized()
    print("Done.")
