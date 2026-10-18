#!/usr/bin/env python3
# SPDX-License-Identifier: OAI-OpenAirInterface
#
# Summarize bounded L1_TX_JOB_DL deadline capture CSV files.
#
# Input CSV format:
#   capture_index,probe_total,frame,slot,duration_us,late_threshold_us,late
#
# Optional context columns, when present:
#   context_valid,dl_pdsch_count,dl_prb_total,dl_tbs_total,dl_mcs_min,
#   dl_mcs_max,dl_mcs_table_min,dl_mcs_table_max,dl_layers_max,
#   dl_rv_nonzero_count

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


REQUIRED_COLUMNS = {
    "capture_index",
    "probe_total",
    "frame",
    "slot",
    "duration_us",
    "late_threshold_us",
    "late",
}


OPTIONAL_INT_COLUMNS = {
    "context_valid",
    "dl_pdsch_count",
    "dl_prb_total",
    "dl_tbs_total",
    "dl_mcs_min",
    "dl_mcs_max",
    "dl_mcs_table_min",
    "dl_mcs_table_max",
    "dl_layers_max",
    "dl_rv_nonzero_count",
}


def ratio_ppm(count: int, total: int) -> int:
    if total == 0:
        return 0
    return int((count * 1_000_000 + total // 2) // total)


def ratio_pct(count: int, total: int) -> float:
    if total == 0:
        return 0.0
    return 100.0 * count / total


def percentile_nearest_rank(sorted_values: Sequence[int], percentile: float) -> int:
    if not sorted_values:
        raise ValueError("empty value list")

    if percentile <= 0:
        return sorted_values[0]
    if percentile >= 100:
        return sorted_values[-1]

    rank = math.ceil((percentile / 100.0) * len(sorted_values))
    index = max(0, min(rank - 1, len(sorted_values) - 1))
    return sorted_values[index]


def load_capture(path: Path) -> List[Dict[str, int]]:
    rows: List[Dict[str, int]] = []

    with path.open(newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError("missing CSV header")

        fieldnames = set(reader.fieldnames)
        missing = REQUIRED_COLUMNS.difference(fieldnames)
        if missing:
            raise ValueError(f"missing required columns: {', '.join(sorted(missing))}")

        optional_columns = OPTIONAL_INT_COLUMNS.intersection(fieldnames)

        for line_no, row in enumerate(reader, start=2):
            try:
                parsed = {
                    "capture_index": int(row["capture_index"]),
                    "probe_total": int(row["probe_total"]),
                    "frame": int(row["frame"]),
                    "slot": int(row["slot"]),
                    "duration_us": int(row["duration_us"]),
                    "late_threshold_us": int(row["late_threshold_us"]),
                    "late": int(row["late"]),
                }

                for column in optional_columns:
                    value = row.get(column, "")
                    if value != "":
                        parsed[column] = int(value)

            except (TypeError, ValueError) as exc:
                raise ValueError(f"invalid integer value at CSV line {line_no}: {exc}") from exc

            rows.append(parsed)

    if not rows:
        raise ValueError("CSV contains no samples")

    return rows


def validate_order(rows: Sequence[Dict[str, int]]) -> List[str]:
    warnings: List[str] = []

    first_capture_index = rows[0]["capture_index"]
    for offset, row in enumerate(rows):
        expected = first_capture_index + offset
        if row["capture_index"] != expected:
            warnings.append(
                f"capture_index discontinuity at sample {offset}: "
                f"expected {expected}, got {row['capture_index']}"
            )
            break

    previous_probe_total = rows[0]["probe_total"]
    for row in rows[1:]:
        if row["probe_total"] <= previous_probe_total:
            warnings.append(
                "probe_total is not strictly increasing "
                f"near capture_index={row['capture_index']}"
            )
            break
        previous_probe_total = row["probe_total"]

    return warnings


def burst_stats(rows: Sequence[Dict[str, int]], threshold_us: int) -> Tuple[int, int, int]:
    burst_count = 0
    longest_burst = 0
    current_burst = 0

    for row in rows:
        if row["duration_us"] > threshold_us:
            current_burst += 1
            if current_burst == 1:
                burst_count += 1
            longest_burst = max(longest_burst, current_burst)
        else:
            current_burst = 0

    over_count = sum(1 for row in rows if row["duration_us"] > threshold_us)
    return over_count, burst_count, longest_burst


def tti_histogram_stats(rows: Sequence[Dict[str, int]], tti_us: int) -> Dict[str, int]:
    if tti_us <= 0:
        raise ValueError("tti_us must be > 0")

    stats = {
        "le_1tti": 0,
        "gt1_le2tti": 0,
        "gt2_le3tti": 0,
        "gt3tti": 0,
    }

    for row in rows:
        duration_us = row["duration_us"]
        if duration_us <= tti_us:
            stats["le_1tti"] += 1
        elif duration_us <= 2 * tti_us:
            stats["gt1_le2tti"] += 1
        elif duration_us <= 3 * tti_us:
            stats["gt2_le3tti"] += 1
        else:
            stats["gt3tti"] += 1

    return stats


def append_tti_histogram_lines(
    lines: List[str],
    rows: Sequence[Dict[str, int]],
    tti_us: int,
    prefix: str = "",
    unit_name: str = "tti",
) -> None:
    total = len(rows)
    durations = [row["duration_us"] for row in rows]
    late_count = sum(1 for row in rows if row["late"] != 0)
    stats = tti_histogram_stats(rows, tti_us)

    if unit_name not in ("tti", "slot"):
        raise ValueError(f"unsupported duration unit name: {unit_name}")

    name = f"{prefix}{unit_name}" if prefix else unit_name

    lines.append(
        f"{name}_histogram "
        f"{unit_name}_us={tti_us} "
        f"samples={total} "
        f"avg_us={sum(durations) / total:.3f} "
        f"max_us={max(durations)} "
        f"max_{unit_name}_equiv={max(durations) / tti_us:.3f} "
        f"late_count={late_count} "
        f"late_ratio_ppm={ratio_ppm(late_count, total)}"
    )

    lines.append(
        f"{name}_bucket "
        f"bucket=<=1_{unit_name} "
        f"count={stats['le_1tti']} "
        f"ratio_ppm={ratio_ppm(stats['le_1tti'], total)} "
        f"ratio_pct={ratio_pct(stats['le_1tti'], total):.6f}"
    )

    lines.append(
        f"{name}_bucket "
        f"bucket=>1_<=2_{unit_name} "
        f"count={stats['gt1_le2tti']} "
        f"ratio_ppm={ratio_ppm(stats['gt1_le2tti'], total)} "
        f"ratio_pct={ratio_pct(stats['gt1_le2tti'], total):.6f}"
    )

    lines.append(
        f"{name}_bucket "
        f"bucket=>2_<=3_{unit_name} "
        f"count={stats['gt2_le3tti']} "
        f"ratio_ppm={ratio_ppm(stats['gt2_le3tti'], total)} "
        f"ratio_pct={ratio_pct(stats['gt2_le3tti'], total):.6f}"
    )

    lines.append(
        f"{name}_bucket "
        f"bucket=>3_{unit_name} "
        f"count={stats['gt3tti']} "
        f"ratio_ppm={ratio_ppm(stats['gt3tti'], total)} "
        f"ratio_pct={ratio_pct(stats['gt3tti'], total):.6f}"
    )


def duration_stats(rows: Sequence[Dict[str, int]]) -> Dict[str, float]:
    durations = [row["duration_us"] for row in rows]
    sorted_durations = sorted(durations)

    return {
        "min_us": min(durations),
        "avg_us": round(sum(durations) / len(durations), 3),
        "max_us": max(durations),
        "p50_us": percentile_nearest_rank(sorted_durations, 50),
        "p90_us": percentile_nearest_rank(sorted_durations, 90),
        "p99_us": percentile_nearest_rank(sorted_durations, 99),
        "p999_us": percentile_nearest_rank(sorted_durations, 99.9),
        "p9999_us": percentile_nearest_rank(sorted_durations, 99.99),
    }


def threshold_summaries(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
) -> List[Dict[str, int]]:
    total = len(rows)
    summaries: List[Dict[str, int]] = []

    for threshold_us in thresholds:
        over_count, burst_count, longest_burst = burst_stats(rows, threshold_us)
        summaries.append(
            {
                "threshold_us": threshold_us,
                "count": over_count,
                "ratio_ppm": ratio_ppm(over_count, total),
                "burst_count": burst_count,
                "longest_burst": longest_burst,
            }
        )

    return summaries


def unit_histogram_summary(
    rows: Sequence[Dict[str, int]],
    unit_us: int,
    unit_name: str,
) -> Dict[str, object]:
    total = len(rows)
    durations = [row["duration_us"] for row in rows]
    late_count = sum(1 for row in rows if row["late"] != 0)
    stats = tti_histogram_stats(rows, unit_us)

    buckets = {
        "le_1": stats["le_1tti"],
        "gt_1_le_2": stats["gt1_le2tti"],
        "gt_2_le_3": stats["gt2_le3tti"],
        "gt_3": stats["gt3tti"],
    }

    return {
        "unit_name": unit_name,
        "unit_us": unit_us,
        "samples": total,
        "avg_us": round(sum(durations) / total, 3),
        "max_us": max(durations),
        "max_unit_equiv": round(max(durations) / unit_us, 3),
        "late_count": late_count,
        "late_ratio_ppm": ratio_ppm(late_count, total),
        "buckets": {
            name: {
                "count": count,
                "ratio_ppm": ratio_ppm(count, total),
                "ratio_pct": round(ratio_pct(count, total), 6),
            }
            for name, count in buckets.items()
        },
    }


def top_outlier_rows(
    rows: Sequence[Dict[str, int]],
    limit: int = 10,
) -> List[Dict[str, int]]:
    fields = [
        "capture_index",
        "probe_total",
        "frame",
        "slot",
        "duration_us",
        "late_threshold_us",
        "late",
        "context_valid",
        "dl_pdsch_count",
        "dl_prb_total",
        "dl_tbs_total",
        "dl_mcs_min",
        "dl_mcs_max",
        "dl_mcs_table_min",
        "dl_mcs_table_max",
        "dl_layers_max",
        "dl_rv_nonzero_count",
    ]

    sorted_rows = sorted(rows, key=lambda row: row["duration_us"], reverse=True)
    return [
        {field: row[field] for field in fields if field in row}
        for row in sorted_rows[:limit]
    ]


def build_json_summary(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
    duration_unit_us: Optional[int],
    duration_unit_name: str,
    csv_path: Path,
) -> Dict[str, object]:
    total = len(rows)
    late_count = sum(1 for row in rows if row["late"] != 0)
    late_thresholds = sorted(set(row["late_threshold_us"] for row in rows))
    durations = duration_stats(rows)
    warnings = validate_order(rows)

    data: Dict[str, object] = {
        "schema": "rt_deadline_l1tx_summary.v1",
        "csv_path": str(csv_path),
        "samples": total,
        "first_capture_index": rows[0]["capture_index"],
        "last_capture_index": rows[-1]["capture_index"],
        "first_probe_total": rows[0]["probe_total"],
        "last_probe_total": rows[-1]["probe_total"],
        "first_frame": rows[0]["frame"],
        "first_slot": rows[0]["slot"],
        "last_frame": rows[-1]["frame"],
        "last_slot": rows[-1]["slot"],
        "duration_us": durations,
        "min_us": durations["min_us"],
        "avg_us": durations["avg_us"],
        "max_us": durations["max_us"],
        "p50_us": durations["p50_us"],
        "p90_us": durations["p90_us"],
        "p99_us": durations["p99_us"],
        "p999_us": durations["p999_us"],
        "p9999_us": durations["p9999_us"],
        "late_threshold_us_values": late_thresholds,
        "late_count": late_count,
        "late_ratio_ppm": ratio_ppm(late_count, total),
        "thresholds": threshold_summaries(rows, thresholds),
        "top_outliers": top_outlier_rows(rows),
        "warnings": warnings,
    }

    if duration_unit_us is not None:
        histogram = unit_histogram_summary(rows, duration_unit_us, duration_unit_name)
        data["duration_unit"] = histogram
        data[f"{duration_unit_name}_us"] = duration_unit_us
        data[f"{duration_unit_name}_bucket_counts"] = {
            name: bucket["count"]
            for name, bucket in histogram["buckets"].items()
        }

    if any("context_valid" in row for row in rows):
        context_valid_rows = [row for row in rows if row.get("context_valid", 0) == 1]
        context_invalid_rows = [row for row in rows if row.get("context_valid", 0) == 0]

        context: Dict[str, object] = {
            "available": True,
            "valid_samples": len(context_valid_rows),
            "invalid_samples": len(context_invalid_rows),
        }

        if duration_unit_us is not None:
            if context_valid_rows:
                context["valid_duration_unit"] = unit_histogram_summary(
                    context_valid_rows,
                    duration_unit_us,
                    duration_unit_name,
                )
            if context_invalid_rows:
                context["invalid_duration_unit"] = unit_histogram_summary(
                    context_invalid_rows,
                    duration_unit_us,
                    duration_unit_name,
                )

        data["context"] = context
    else:
        data["context"] = {"available": False}

    return data


def write_json_summary(path: Path, data: Dict[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def summarize(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
    tti_us: Optional[int],
    duration_unit_name: str = "tti",
) -> str:
    durations = [row["duration_us"] for row in rows]
    sorted_durations = sorted(durations)
    total = len(rows)

    late_count = sum(1 for row in rows if row["late"] != 0)
    late_thresholds = sorted(set(row["late_threshold_us"] for row in rows))

    lines: List[str] = []
    lines.append("RT_DEADLINE_CAPTURE_SUMMARY")
    lines.append(f"samples={total}")
    lines.append(f"first_capture_index={rows[0]['capture_index']}")
    lines.append(f"last_capture_index={rows[-1]['capture_index']}")
    lines.append(f"first_probe_total={rows[0]['probe_total']}")
    lines.append(f"last_probe_total={rows[-1]['probe_total']}")
    lines.append(f"first_frame={rows[0]['frame']}")
    lines.append(f"first_slot={rows[0]['slot']}")
    lines.append(f"last_frame={rows[-1]['frame']}")
    lines.append(f"last_slot={rows[-1]['slot']}")
    lines.append(f"min_us={min(durations)}")
    lines.append(f"avg_us={sum(durations) / total:.3f}")
    lines.append(f"max_us={max(durations)}")
    lines.append(f"p50_us={percentile_nearest_rank(sorted_durations, 50)}")
    lines.append(f"p90_us={percentile_nearest_rank(sorted_durations, 90)}")
    lines.append(f"p99_us={percentile_nearest_rank(sorted_durations, 99)}")
    lines.append(f"p999_us={percentile_nearest_rank(sorted_durations, 99.9)}")
    lines.append(f"p9999_us={percentile_nearest_rank(sorted_durations, 99.99)}")
    lines.append(f"late_threshold_us_values={','.join(str(x) for x in late_thresholds)}")
    lines.append(f"late_count={late_count}")
    lines.append(f"late_ratio_ppm={ratio_ppm(late_count, total)}")

    for threshold_us in thresholds:
        over_count, burst_count, longest_burst = burst_stats(rows, threshold_us)
        lines.append(
            "threshold "
            f"threshold_us={threshold_us} "
            f"count={over_count} "
            f"ratio_ppm={ratio_ppm(over_count, total)} "
            f"burst_count={burst_count} "
            f"longest_burst={longest_burst}"
        )

    if tti_us is not None:
        append_tti_histogram_lines(lines, rows, tti_us, unit_name=duration_unit_name)

        if any("context_valid" in row for row in rows):
            context_valid_rows = [row for row in rows if row.get("context_valid", 0) == 1]
            context_invalid_rows = [row for row in rows if row.get("context_valid", 0) == 0]

            if context_valid_rows:
                append_tti_histogram_lines(
                    lines,
                    context_valid_rows,
                    tti_us,
                    prefix="context_valid_",
                    unit_name=duration_unit_name,
                )

            if context_invalid_rows:
                append_tti_histogram_lines(
                    lines,
                    context_invalid_rows,
                    tti_us,
                    prefix="context_invalid_",
                    unit_name=duration_unit_name,
                )

    warnings = validate_order(rows)
    for warning in warnings:
        lines.append(f"warning={warning}")

    return "\n".join(lines)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize L1_TX_JOB_DL deadline capture CSV samples."
    )
    parser.add_argument(
        "csv_path",
        type=Path,
        help="Path to rt_deadline_l1tx_samples.csv",
    )
    parser.add_argument(
        "--deadline-us",
        "--threshold",
        dest="thresholds",
        type=int,
        action="append",
        default=None,
        help="Deadline in microseconds. Can be provided multiple times.",
    )
    parser.add_argument(
        "--start-index",
        type=int,
        default=None,
        help="Keep samples whose capture_index is greater than or equal to this value.",
    )
    parser.add_argument(
        "--end-index",
        type=int,
        default=None,
        help="Keep samples whose capture_index is less than or equal to this value.",
    )

    parser.add_argument(
        "--plot-ecdf",
        action="store_true",
        help="Write an ECDF plot of duration_us.",
    )
    parser.add_argument(
        "--plot-ccdf",
        action="store_true",
        help="Write a logarithmic CCDF plot of duration_us.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("analysis_outputs"),
        help="Directory used for generated plots.",
    )
    parser.add_argument(
        "--ecdf-x-max-us",
        type=int,
        default=None,
        help="Optional upper bound for the ECDF x-axis in microseconds.",
    )
    parser.add_argument(
        "--ccdf-x-max-us",
        type=int,
        default=None,
        help="Optional upper bound for the CCDF x-axis in microseconds.",
    )

    duration_unit = parser.add_mutually_exclusive_group()
    duration_unit.add_argument(
        "--tti-us",
        type=int,
        default=None,
        help=(
            "Report duration histogram in TTI units using the provided TTI "
            "duration in microseconds, for example --tti-us 1000."
        ),
    )
    duration_unit.add_argument(
        "--slot-us",
        type=int,
        default=None,
        help=(
            "Report duration histogram in slot units using the provided slot "
            "duration in microseconds, for example --slot-us 500."
        ),
    )
    parser.add_argument(
        "--json-summary",
        type=Path,
        default=None,
        help=(
            "Write a machine-readable JSON summary to the provided path while "
            "keeping the text summary on stdout."
        ),
    )
    return parser.parse_args()


def filter_rows_by_capture_index(
    rows: List[Dict[str, int]],
    start_index: Optional[int],
    end_index: Optional[int],
) -> List[Dict[str, int]]:
    if start_index is not None and start_index < 0:
        raise ValueError("--start-index must be >= 0")
    if end_index is not None and end_index < 0:
        raise ValueError("--end-index must be >= 0")
    if (
        start_index is not None
        and end_index is not None
        and start_index > end_index
    ):
        raise ValueError("--start-index must be <= --end-index")

    filtered = [
        row
        for row in rows
        if (start_index is None or row["capture_index"] >= start_index)
        and (end_index is None or row["capture_index"] <= end_index)
    ]

    if not filtered:
        raise ValueError("selected capture-index window contains no samples")

    return filtered


def write_ecdf_plot(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
    output_dir: Path,
    x_max_us: Optional[int],
) -> Path:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise RuntimeError(
            "matplotlib is required when using --plot-ecdf"
        ) from exc

    if x_max_us is not None and x_max_us <= 0:
        raise ValueError("--ecdf-x-max-us must be > 0")

    durations = sorted(row["duration_us"] for row in rows)
    cumulative = [
        (index + 1) / len(durations)
        for index in range(len(durations))
    ]

    output_dir.mkdir(parents=True, exist_ok=True)

    output_name = "l1tx_duration_ecdf.png"
    if x_max_us is not None:
        output_name = f"l1tx_duration_ecdf_xmax_{x_max_us}us.png"

    output_path = output_dir / output_name

    figure, axis = plt.subplots()
    axis.step(durations, cumulative, where="post", label="L1_TX_JOB_DL")

    visible_thresholds = [
        threshold_us
        for threshold_us in thresholds
        if x_max_us is None or threshold_us <= x_max_us
    ]

    for threshold_us in visible_thresholds:
        axis.axvline(
            threshold_us,
            linestyle="--",
            linewidth=1,
            label=f"{threshold_us} us",
        )

    axis.set_xlabel("Processing duration (us)")
    axis.set_ylabel("Empirical cumulative probability")
    axis.set_ylim(0.0, 1.0)

    if x_max_us is not None:
        axis.set_xlim(right=x_max_us)

    axis.legend()
    figure.tight_layout()
    figure.savefig(output_path, dpi=160)
    plt.close(figure)

    return output_path


def write_ccdf_plot(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
    output_dir: Path,
    x_max_us: Optional[int],
) -> Path:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise RuntimeError(
            "matplotlib is required when using --plot-ccdf"
        ) from exc

    if x_max_us is not None and x_max_us <= 0:
        raise ValueError("--ccdf-x-max-us must be > 0")

    durations = sorted(row["duration_us"] for row in rows)
    sample_count = len(durations)

    survival = [
        (sample_count - index) / sample_count
        for index in range(sample_count)
    ]

    output_dir.mkdir(parents=True, exist_ok=True)

    output_name = "l1tx_duration_ccdf.png"
    if x_max_us is not None:
        output_name = f"l1tx_duration_ccdf_xmax_{x_max_us}us.png"

    output_path = output_dir / output_name

    figure, axis = plt.subplots()
    axis.step(
        durations,
        survival,
        where="post",
        label="L1_TX_JOB_DL",
    )

    visible_thresholds = [
        threshold_us
        for threshold_us in thresholds
        if x_max_us is None or threshold_us <= x_max_us
    ]

    for threshold_us in visible_thresholds:
        axis.axvline(
            threshold_us,
            linestyle="--",
            linewidth=1,
            label=f"{threshold_us} us",
        )

    axis.set_xlabel("Processing duration (us)")
    axis.set_ylabel("Survival probability P(duration >= x)")
    axis.set_yscale("log")
    axis.set_ylim(
        bottom=max(1.0 / (2.0 * sample_count), 1e-7),
        top=1.0,
    )

    if x_max_us is not None:
        axis.set_xlim(right=x_max_us)

    axis.legend()
    figure.tight_layout()
    figure.savefig(output_path, dpi=160)
    plt.close(figure)

    return output_path


def resolve_duration_unit(args: argparse.Namespace) -> Tuple[Optional[int], str]:
    if args.slot_us is not None:
        if args.slot_us <= 0:
            raise ValueError("--slot-us must be > 0")
        return args.slot_us, "slot"

    if args.tti_us is not None:
        if args.tti_us <= 0:
            raise ValueError("--tti-us must be > 0")
        return args.tti_us, "tti"

    return None, "tti"


def main() -> int:
    args = parse_args()

    thresholds = args.thresholds if args.thresholds is not None else [200, 500, 1000, 2000]
    thresholds = sorted(set(thresholds))

    try:
        rows = load_capture(args.csv_path)
        rows = filter_rows_by_capture_index(
            rows,
            args.start_index,
            args.end_index,
        )
        duration_unit_us, duration_unit_name = resolve_duration_unit(args)
        print(summarize(rows, thresholds, duration_unit_us, duration_unit_name))

        if args.plot_ecdf:
            output_path = write_ecdf_plot(
                rows,
                thresholds,
                args.output_dir,
                args.ecdf_x_max_us,
            )
            print(f"ecdf_plot={output_path}")

        if args.plot_ccdf:
            output_path = write_ccdf_plot(
                rows,
                thresholds,
                args.output_dir,
                args.ccdf_x_max_us,
            )
            print(f"ccdf_plot={output_path}")

        if args.json_summary is not None:
            write_json_summary(
                args.json_summary,
                build_json_summary(
                    rows,
                    thresholds,
                    duration_unit_us,
                    duration_unit_name,
                    args.csv_path,
                ),
            )
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
