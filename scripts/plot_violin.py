#!/usr/bin/env python3
"""Violin plots comparing hop-latency distributions across latency_tests runs.

Reads every `latency_*.csv` (NOT `*_summary.csv`) under a results directory,
parses the `#`-prefixed metadata header for run config, and produces one or
more violin plots grouped by RMW implementation. Per-run metadata is used to
build the plot title and the x-axis tick labels.

Usage:
    ./scripts/plot_violin.py                               # plot every CSV in data/results
    ./scripts/plot_violin.py --results-dir data/results    # explicit dir
    ./scripts/plot_violin.py --filter PoseStamped          # only files matching substring
    ./scripts/plot_violin.py --group-by msg_payload        # see --help
    ./scripts/plot_violin.py --out plot.png                # override output path

Requires: pandas, matplotlib (seaborn optional, nicer styling if present).
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import glob
import os
import sys
from typing import Dict, List, Optional

import pandas as pd
import matplotlib.pyplot as plt

try:
    import seaborn as sns
    _HAS_SNS = True
except ImportError:
    _HAS_SNS = False


@dataclasses.dataclass
class RunCsv:
    path: str
    meta: Dict[str, str]
    hop_ns: pd.Series  # hop_ns samples for every per-message row

    @property
    def rmw(self) -> str:
        return self.meta.get("rmw", "unknown")

    @property
    def msg(self) -> str:
        return self.meta.get("message_type", "unknown")

    @property
    def payload(self) -> str:
        return self.meta.get("payload_bytes", "?")

    @property
    def nodes(self) -> str:
        return self.meta.get("num_nodes", "?")

    @property
    def threads(self) -> str:
        return self.meta.get("num_threads", "?")

    @property
    def rate(self) -> str:
        return self.meta.get("publish_rate_hz", "?")

    @property
    def ipc(self) -> bool:
        return self.meta.get("use_intra_process_comms", "false").lower() == "true"

    @property
    def cpu_model(self) -> str:
        return self.meta.get("cpu_model", "unknown CPU")

    @property
    def kernel(self) -> str:
        return self.meta.get("kernel", "unknown kernel")

    @property
    def os_pretty(self) -> str:
        return self.meta.get("os", "unknown OS")

    def label(self, scheme: str) -> str:
        parts: List[str] = []
        if "rmw" in scheme:
            parts.append(_short_rmw(self.rmw))
        if "msg" in scheme:
            parts.append(self.msg.split("/")[-1])
        if "payload" in scheme:
            parts.append(f"{_human_bytes(int(self.payload))}")
        if "nodes" in scheme:
            parts.append(f"{self.nodes}n")
        if "threads" in scheme:
            parts.append(f"{self.threads}t")
        if "ipc" in scheme:
            parts.append("ipc" if self.ipc else "no-ipc")
        return "\n".join(parts) if parts else os.path.basename(self.path)


def _short_rmw(rmw: str) -> str:
    return (
        rmw.replace("rmw_", "")
           .replace("_cpp", "")
           .replace("rtps", "dds")
    )


def _human_bytes(n: int) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return f"{n}{unit}"
        n //= 1024
    return f"{n}TB"


def _parse_meta(path: str) -> Dict[str, str]:
    meta: Dict[str, str] = {}
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            if not line.startswith("#"):
                break
            kv = line[1:].strip()
            if "=" not in kv:
                continue
            k, v = kv.split("=", 1)
            meta[k.strip()] = v.strip()
    return meta


def _load_csv(path: str) -> Optional[RunCsv]:
    meta = _parse_meta(path)
    if not meta:
        print(f"[warn] no metadata header in {path}, skipping", file=sys.stderr)
        return None
    # The CSV header row is the first non-# line; let pandas skip comments.
    try:
        df = pd.read_csv(path, comment="#")
    except Exception as e:
        print(f"[warn] failed to read {path}: {e}", file=sys.stderr)
        return None
    if "hop_ns" not in df.columns or "node_index" not in df.columns:
        print(f"[warn] {path} missing expected columns, skipping", file=sys.stderr)
        return None
    # node_index 0 is the publisher — its hop is always 0 (no predecessor).
    # Drop it so the distribution only contains real inter-node hops.
    hops = df.loc[df["node_index"] > 0, "hop_ns"].dropna()
    return RunCsv(path=path, meta=meta, hop_ns=hops)


def _find_csvs(results_dir: str, filt: Optional[str]) -> List[str]:
    pattern = os.path.join(results_dir, "latency_*.csv")
    paths = sorted(p for p in glob.glob(pattern) if not p.endswith("_summary.csv"))
    if filt:
        paths = [p for p in paths if filt in os.path.basename(p)]
    return paths


def _short_cpu(cpu: str) -> str:
    # "12th Gen Intel(R) Core(TM) i7-12800H" -> "Intel Core i7-12800H"
    s = cpu.replace("(R)", "").replace("(TM)", "")
    s = " ".join(s.split())
    return s


def _short_kernel(kernel: str) -> str:
    # "Linux 5.15.0-139-generic x86_64" -> "Linux 5.15.0-139-generic"
    parts = kernel.split()
    return " ".join(parts[:2]) if len(parts) >= 2 else kernel


def _host_subtitle(runs: List[RunCsv]) -> str:
    """Build a one-line host summary from the CSV metadata (NOT the plotting
    host — these may differ). If all runs share the same host info, collapse
    to a single line; otherwise list unique combinations."""
    combos = []
    seen = set()
    for r in runs:
        key = (_short_cpu(r.cpu_model), r.os_pretty, _short_kernel(r.kernel))
        if key in seen:
            continue
        seen.add(key)
        combos.append(key)
    if len(combos) == 1:
        cpu, os_name, kern = combos[0]
        return f"{cpu}  •  {os_name}  •  {kern}"
    return " | ".join(f"{cpu} / {os_name} / {kern}" for cpu, os_name, kern in combos)


def _build_dataframe(runs: List[RunCsv], label_scheme: str) -> pd.DataFrame:
    frames = []
    for r in runs:
        if r.hop_ns.empty:
            continue
        frames.append(pd.DataFrame({
            "hop_us": r.hop_ns.values / 1000.0,
            "run": r.label(label_scheme),
            "rmw": _short_rmw(r.rmw),
            "msg": r.msg.split("/")[-1],
        }))
    if not frames:
        return pd.DataFrame(columns=["hop_us", "run", "rmw", "msg"])
    return pd.concat(frames, ignore_index=True)


def _plot(df: pd.DataFrame, title: str, subtitle: str, out_path: str, log: bool) -> None:
    if df.empty:
        print("[error] no samples to plot", file=sys.stderr)
        sys.exit(2)

    n_runs = df["run"].nunique()
    fig_h = max(6.0, 1.1 * n_runs + 2.0)
    fig, ax = plt.subplots(figsize=(10.0, fig_h))

    if _HAS_SNS:
        # Sort rows so runs of the same message type cluster together on the
        # y axis. The default label scheme puts msg first, so lexicographic
        # sort groups by msg-major.
        row_order = (
            df[["run", "msg"]]
            .drop_duplicates()
            .sort_values(["msg", "run"])["run"]
            .tolist()
        )
        sns.violinplot(
            data=df, y="run", x="hop_us", hue="rmw",
            order=row_order,
            cut=0, inner="quartile", linewidth=1.0,
            density_norm="width", ax=ax, legend="brief",
            alpha=0.7, orient="h", width=0.9, gap=0.25,
        )
    else:
        # matplotlib fallback: one violin per unique run
        groups = [g["hop_us"].values for _, g in df.groupby("run", sort=False)]
        labels = list(df["run"].drop_duplicates())
        parts = ax.violinplot(groups, showmedians=True, widths=0.55, vert=False)
        for body in parts["bodies"]:
            body.set_alpha(0.7)
        ax.set_yticks(range(1, len(labels) + 1))
        ax.set_yticklabels(labels)

    ax.set_xlabel("hop latency (µs)")
    ax.set_ylabel("")
    if subtitle:
        ax.set_title(f"{title}\n{subtitle}", fontsize=11)
    else:
        ax.set_title(title)
    if log:
        ax.set_xscale("log")
        ax.set_xlim(left=1.0)  # 1 µs
    else:
        ax.set_xlim(left=0)
    ax.grid(True, axis="x", which="both", linestyle=":", alpha=0.6)

    fig.tight_layout()
    fig.savefig(out_path, dpi=140)
    print(f"[ok] wrote {out_path}", file=sys.stderr)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--results-dir", default="/data/results",
                    help="directory of latency_*.csv files (container path)")
    ap.add_argument("--filter", default=None,
                    help="only include CSVs whose filename contains this substring")
    ap.add_argument("--group-by", default="msg_payload_ipc",
                    help="underscore-separated subset of "
                         "{rmw,msg,payload,nodes,threads,ipc}"
                         " used as the run-label axis (rmw is encoded by "
                         "violin colour via the hue, so it's omitted by "
                         "default; rows are sorted msg-major so runs of the "
                         "same message type sit together)")
    ap.add_argument("--out", default=None,
                    help="output PNG path (default: data/results/violin_<ts>.png)")
    ap.add_argument("--title", default=None, help="override plot title")
    ap.add_argument("--linear", dest="log", action="store_false",
                    help="use a linear y axis (min=0) instead of the default log scale")
    ap.set_defaults(log=True)
    args = ap.parse_args()

    paths = _find_csvs(args.results_dir, args.filter)
    if not paths:
        print(f"[error] no matching CSVs in {args.results_dir}", file=sys.stderr)
        sys.exit(1)

    runs = [r for r in (_load_csv(p) for p in paths) if r is not None]
    if not runs:
        print("[error] no loadable CSVs", file=sys.stderr)
        sys.exit(1)

    df = _build_dataframe(runs, args.group_by)

    title = args.title or f"Hop latency distribution ({len(runs)} runs)"
    subtitle = _host_subtitle(runs)

    if args.out:
        out_path = args.out
    else:
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        suffix = args.filter or "all"
        out_path = os.path.join(args.results_dir, f"violin_{suffix}_{ts}.png")

    _plot(df, title, subtitle, out_path, args.log)


if __name__ == "__main__":
    main()
