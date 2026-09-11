#!/usr/bin/env python3
# SPDX-License-Identifier: OAI-OpenAirInterface
#
# Summarize L1_RX_JOB_UL deadline capture CSV files.
#
# The L1 RX V1 CSV schema contains 23 columns:
#   capture_index,probe_total,frame,slot,duration_us,late_threshold_us,late,
#   context_valid,ul_pucch_job_count,ul_pusch_job_count,ul_pusch_data_count,
#   ul_pusch_decode_count,ul_srs_job_count,ul_pusch_prb_total,
#   ul_pusch_tbs_total,ul_pusch_mcs_min,ul_pusch_mcs_max,
#   ul_pusch_mcs_table_min,ul_pusch_mcs_table_max,ul_pusch_layers_max,
#   ul_pusch_rv_nonzero_count,ul_crc_ok_count,ul_crc_fail_count
#
# Important semantics:
# - ul_pusch_job_count counts current-slot PUSCH jobs extracted for processing.
# - ul_pusch_decode_count counts PUSCH jobs passed to nr_ulsch_procedures().
# - CRC FAIL indications may also be produced before nr_ulsch_procedures(),
#   notably for PUSCH not detected / DTX.
# - Therefore CRC count > decode count is diagnostic, not an error.
#
# Runtime thresholds such as 1000 us are observation thresholds. They are not
# automatically protocol deadlines.

import argparse
import csv
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

from analyze_l1tx_capture import (
    append_tti_histogram_lines,
    duration_stats,
    filter_rows_by_capture_index,
    ratio_ppm,
    resolve_duration_unit,
    threshold_summaries,
    unit_histogram_summary,
    validate_order,
    write_json_summary,
)

EXPECTED_COLUMNS = [
    "capture_index",
    "probe_total",
    "frame",
    "slot",
    "duration_us",
    "late_threshold_us",
    "late",
    "context_valid",
    "ul_pucch_job_count",
    "ul_pusch_job_count",
    "ul_pusch_data_count",
    "ul_pusch_decode_count",
    "ul_srs_job_count",
    "ul_pusch_prb_total",
    "ul_pusch_tbs_total",
    "ul_pusch_mcs_min",
    "ul_pusch_mcs_max",
    "ul_pusch_mcs_table_min",
    "ul_pusch_mcs_table_max",
    "ul_pusch_layers_max",
    "ul_pusch_rv_nonzero_count",
    "ul_crc_ok_count",
    "ul_crc_fail_count",
]

ZERO_CONTEXT_FIELDS = [
    "ul_pucch_job_count",
    "ul_pusch_job_count",
    "ul_pusch_data_count",
    "ul_pusch_decode_count",
    "ul_srs_job_count",
    "ul_pusch_prb_total",
    "ul_pusch_tbs_total",
    "ul_pusch_rv_nonzero_count",
    "ul_crc_ok_count",
    "ul_crc_fail_count",
]

PUSCH_ZERO_FIELDS = [
    "ul_pusch_data_count",
    "ul_pusch_decode_count",
    "ul_pusch_prb_total",
    "ul_pusch_tbs_total",
    "ul_pusch_rv_nonzero_count",
    "ul_crc_ok_count",
    "ul_crc_fail_count",
]

PUSCH_SENTINEL_FIELDS = [
    "ul_pusch_mcs_min",
    "ul_pusch_mcs_max",
    "ul_pusch_mcs_table_min",
    "ul_pusch_mcs_table_max",
    "ul_pusch_layers_max",
]


def load_capture(path: Path) -> List[Dict[str, int]]:
    rows: List[Dict[str, int]] = []

    with path.open(newline="") as f:
        reader = csv.DictReader(f)

        if reader.fieldnames is None:
            raise ValueError("missing CSV header")

        if reader.fieldnames != EXPECTED_COLUMNS:
            actual = set(reader.fieldnames)
            expected = set(EXPECTED_COLUMNS)
            missing = sorted(expected - actual)
            unexpected = sorted(actual - expected)

            details = []
            if missing:
                details.append(f"missing={','.join(missing)}")
            if unexpected:
                details.append(f"unexpected={','.join(unexpected)}")
            if not missing and not unexpected:
                details.append("column_order_mismatch")

            raise ValueError(
                "unexpected L1 RX CSV schema: " + " ".join(details)
            )

        for line_no, row in enumerate(reader, start=2):
            try:
                parsed = {
                    column: int(row[column])
                    for column in EXPECTED_COLUMNS
                }
            except (TypeError, ValueError) as exc:
                raise ValueError(
                    f"invalid integer value at CSV line {line_no}: {exc}"
                ) from exc

            rows.append(parsed)

    if not rows:
        raise ValueError("CSV contains no samples")

    return rows


def top_outlier_rows(
    rows: Sequence[Dict[str, int]],
    limit: int = 10,
) -> List[Dict[str, int]]:
    sorted_rows = sorted(
        rows,
        key=lambda row: row["duration_us"],
        reverse=True,
    )

    return [
        {column: row[column] for column in EXPECTED_COLUMNS}
        for row in sorted_rows[:limit]
    ]


def semantic_checks(
    rows: Sequence[Dict[str, int]],
) -> Dict[str, object]:
    errors = {
        "capture_index_discontinuity": 0,
        "probe_total_not_strictly_increasing": 0,
        "probe_total_offset_mismatch": 0,
        "late_not_binary": 0,
        "late_mismatch": 0,
        "negative_duration": 0,
        "negative_late_threshold": 0,
        "context_valid_not_binary": 0,
        "negative_workload_counter": 0,
        "invalid_context_sentinel_errors": 0,
        "no_pusch_sentinel_errors": 0,
        "data_gt_jobs": 0,
        "decode_gt_jobs": 0,
        "rv_gt_data": 0,
        "crc_lt_decode": 0,
        "crc_gt_jobs": 0,
        "pusch_context_range_errors": 0,
        "no_data_payload_errors": 0,
    }

    predecode_crc_rows = 0
    predecode_crc_indications = 0

    probe_total_capture_offset = (
        rows[0]["probe_total"] - rows[0]["capture_index"]
    )

    previous_capture_index = None
    previous_probe_total = None

    nonnegative_context_fields = [
        "ul_pucch_job_count",
        "ul_pusch_job_count",
        "ul_pusch_data_count",
        "ul_pusch_decode_count",
        "ul_srs_job_count",
        "ul_pusch_prb_total",
        "ul_pusch_tbs_total",
        "ul_pusch_rv_nonzero_count",
        "ul_crc_ok_count",
        "ul_crc_fail_count",
    ]

    for row in rows:
        capture_index = row["capture_index"]
        probe_total = row["probe_total"]

        if (
            previous_capture_index is not None
            and capture_index != previous_capture_index + 1
        ):
            errors["capture_index_discontinuity"] += 1

        if (
            previous_probe_total is not None
            and probe_total <= previous_probe_total
        ):
            errors["probe_total_not_strictly_increasing"] += 1

        if (
            probe_total - capture_index
            != probe_total_capture_offset
        ):
            errors["probe_total_offset_mismatch"] += 1

        previous_capture_index = capture_index
        previous_probe_total = probe_total

        duration_us = row["duration_us"]
        late_threshold_us = row["late_threshold_us"]
        late = row["late"]

        if duration_us < 0:
            errors["negative_duration"] += 1

        if late_threshold_us < 0:
            errors["negative_late_threshold"] += 1

        if late not in (0, 1):
            errors["late_not_binary"] += 1
        else:
            expected_late = int(
                late_threshold_us > 0
                and duration_us > late_threshold_us
            )

            if late != expected_late:
                errors["late_mismatch"] += 1

        context_valid = row["context_valid"]

        if context_valid not in (0, 1):
            errors["context_valid_not_binary"] += 1
            continue

        if context_valid == 0:
            invalid = (
                any(row[field] != 0 for field in ZERO_CONTEXT_FIELDS)
                or any(row[field] != -1 for field in PUSCH_SENTINEL_FIELDS)
            )

            if invalid:
                errors["invalid_context_sentinel_errors"] += 1

            continue

        if any(
            row[field] < 0
            for field in nonnegative_context_fields
        ):
            errors["negative_workload_counter"] += 1

        jobs = row["ul_pusch_job_count"]
        data = row["ul_pusch_data_count"]
        decode = row["ul_pusch_decode_count"]
        rv_nonzero = row["ul_pusch_rv_nonzero_count"]
        crc_total = (
            row["ul_crc_ok_count"]
            + row["ul_crc_fail_count"]
        )

        if jobs == 0:
            invalid = (
                any(row[field] != 0 for field in PUSCH_ZERO_FIELDS)
                or any(row[field] != -1 for field in PUSCH_SENTINEL_FIELDS)
            )

            if invalid:
                errors["no_pusch_sentinel_errors"] += 1

        else:
            mcs_min = row["ul_pusch_mcs_min"]
            mcs_max = row["ul_pusch_mcs_max"]
            table_min = row["ul_pusch_mcs_table_min"]
            table_max = row["ul_pusch_mcs_table_max"]
            layers_max = row["ul_pusch_layers_max"]

            invalid_range = (
                mcs_min < 0
                or mcs_max < 0
                or mcs_min > mcs_max
                or table_min < 0
                or table_max < 0
                or table_min > table_max
                or layers_max < 1
            )

            if invalid_range:
                errors["pusch_context_range_errors"] += 1

        if data > jobs:
            errors["data_gt_jobs"] += 1

        if decode > jobs:
            errors["decode_gt_jobs"] += 1

        if rv_nonzero > data:
            errors["rv_gt_data"] += 1

        if data == 0 and (
            row["ul_pusch_tbs_total"] != 0
            or rv_nonzero != 0
        ):
            errors["no_data_payload_errors"] += 1

        # A PUSCH passed to nr_ulsch_procedures() produces a CRC indication.
        if crc_total < decode:
            errors["crc_lt_decode"] += 1

        # A current-slot PUSCH job can produce at most one CRC indication.
        if crc_total > jobs:
            errors["crc_gt_jobs"] += 1

        # Diagnostic only: a CRC indication may be emitted before
        # nr_ulsch_procedures(), notably for PUSCH not detected / DTX.
        if crc_total > decode:
            predecode_crc_rows += 1
            predecode_crc_indications += crc_total - decode

    error_total = sum(errors.values())

    return {
        "validation": "PASS" if error_total == 0 else "FAIL",
        "error_total": error_total,
        "errors": errors,
        "diagnostics": {
            "probe_total_capture_offset": probe_total_capture_offset,
            "predecode_crc_rows": predecode_crc_rows,
            "predecode_crc_indications": predecode_crc_indications,
        },
    }


def workload_summary(
    rows: Sequence[Dict[str, int]],
) -> Dict[str, int]:
    valid_rows = [
        row for row in rows
        if row["context_valid"] == 1
    ]
    invalid_rows = [
        row for row in rows
        if row["context_valid"] == 0
    ]

    pusch_rows = [
        row for row in valid_rows
        if row["ul_pusch_job_count"] > 0
    ]

    data_rows = [
        row for row in valid_rows
        if row["ul_pusch_data_count"] > 0
    ]

    decode_rows = [
        row for row in valid_rows
        if row["ul_pusch_decode_count"] > 0
    ]

    crc_ok_rows = [
        row for row in valid_rows
        if row["ul_crc_ok_count"] > 0
    ]

    crc_fail_rows = [
        row for row in valid_rows
        if row["ul_crc_fail_count"] > 0
    ]

    predecode_rows = [
        row for row in pusch_rows
        if (
            row["ul_crc_ok_count"]
            + row["ul_crc_fail_count"]
            > row["ul_pusch_decode_count"]
        )
    ]

    return {
        "context_valid_samples": len(valid_rows),
        "context_invalid_samples": len(invalid_rows),

        "pucch_rows": sum(
            row["ul_pucch_job_count"] > 0
            for row in valid_rows
        ),
        "pusch_rows": len(pusch_rows),
        "pusch_data_rows": len(data_rows),
        "pusch_decode_rows": len(decode_rows),
        "srs_rows": sum(
            row["ul_srs_job_count"] > 0
            for row in valid_rows
        ),

        "pucch_jobs_total": sum(
            row["ul_pucch_job_count"]
            for row in valid_rows
        ),
        "pusch_jobs_total": sum(
            row["ul_pusch_job_count"]
            for row in valid_rows
        ),
        "pusch_data_total": sum(
            row["ul_pusch_data_count"]
            for row in valid_rows
        ),
        "pusch_decode_total": sum(
            row["ul_pusch_decode_count"]
            for row in valid_rows
        ),
        "srs_jobs_total": sum(
            row["ul_srs_job_count"]
            for row in valid_rows
        ),

        "multi_pusch_rows": sum(
            row["ul_pusch_job_count"] > 1
            for row in valid_rows
        ),
        "pusch_no_decode_rows": sum(
            row["ul_pusch_job_count"] > 0
            and row["ul_pusch_decode_count"] == 0
            for row in valid_rows
        ),

        "crc_ok_rows": len(crc_ok_rows),
        "crc_fail_rows": len(crc_fail_rows),
        "crc_ok_indications": sum(
            row["ul_crc_ok_count"]
            for row in valid_rows
        ),
        "crc_fail_indications": sum(
            row["ul_crc_fail_count"]
            for row in valid_rows
        ),

        "predecode_crc_rows": len(predecode_rows),
        "predecode_crc_indications": sum(
            (
                row["ul_crc_ok_count"]
                + row["ul_crc_fail_count"]
                - row["ul_pusch_decode_count"]
            )
            for row in predecode_rows
        ),

        "late_pusch_rows": sum(
            row["late"] != 0
            for row in pusch_rows
        ),

        "max_pusch_duration_us": (
            max(row["duration_us"] for row in pusch_rows)
            if pusch_rows
            else 0
        ),
    }


def timing_subset_summary(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
) -> Dict[str, object]:
    if not rows:
        return {
            "samples": 0,
        }

    late_count = sum(
        row["late"] != 0
        for row in rows
    )

    return {
        "samples": len(rows),
        "duration_us": duration_stats(rows),
        "late_count": late_count,
        "late_ratio_ppm": ratio_ppm(late_count, len(rows)),
        "thresholds": threshold_summaries(rows, thresholds),
    }


def timing_by_context(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
) -> Dict[str, object]:
    valid_rows = [
        row for row in rows
        if row["context_valid"] == 1
    ]

    invalid_rows = [
        row for row in rows
        if row["context_valid"] == 0
    ]

    no_pusch = [
        row for row in valid_rows
        if row["ul_pusch_job_count"] == 0
    ]

    pusch = [
        row for row in valid_rows
        if row["ul_pusch_job_count"] > 0
    ]

    decode = [
        row for row in valid_rows
        if row["ul_pusch_decode_count"] > 0
    ]

    crc_fail = [
        row for row in valid_rows
        if row["ul_crc_fail_count"] > 0
    ]

    predecode_crc = [
        row for row in valid_rows
        if (
            row["ul_crc_ok_count"]
            + row["ul_crc_fail_count"]
            > row["ul_pusch_decode_count"]
        )
    ]

    return {
        "context_invalid": timing_subset_summary(
            invalid_rows,
            thresholds,
        ),
        "no_pusch": timing_subset_summary(
            no_pusch,
            thresholds,
        ),
        "pusch": timing_subset_summary(
            pusch,
            thresholds,
        ),
        "decode": timing_subset_summary(
            decode,
            thresholds,
        ),
        "crc_fail": timing_subset_summary(
            crc_fail,
            thresholds,
        ),
        "predecode_crc": timing_subset_summary(
            predecode_crc,
            thresholds,
        ),
    }


def build_json_summary(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
    duration_unit_us: Optional[int],
    duration_unit_name: str,
    csv_path: Path,
) -> Dict[str, object]:
    total = len(rows)
    durations = duration_stats(rows)
    late_count = sum(
        row["late"] != 0
        for row in rows
    )
    late_thresholds = sorted(
        set(row["late_threshold_us"] for row in rows)
    )

    data: Dict[str, object] = {
        "schema": "rt_deadline_l1rx_summary.v1",
        "probe": "L1_RX_JOB_UL",
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
        "late_ratio_ppm": ratio_ppm(
            late_count,
            total,
        ),

        "thresholds": threshold_summaries(
            rows,
            thresholds,
        ),

        "workload": workload_summary(rows),
        "semantics": semantic_checks(rows),
        "timing_by_context": timing_by_context(
            rows,
            thresholds,
        ),

        "top_outliers": top_outlier_rows(rows),
        "warnings": validate_order(rows),
    }

    if duration_unit_us is not None:
        histogram = unit_histogram_summary(
            rows,
            duration_unit_us,
            duration_unit_name,
        )

        data["duration_unit"] = histogram
        data[f"{duration_unit_name}_us"] = duration_unit_us
        data[f"{duration_unit_name}_bucket_counts"] = {
            name: bucket["count"]
            for name, bucket in histogram["buckets"].items()
        }

    return data


def append_context_timing_lines(
    lines: List[str],
    rows: Sequence[Dict[str, int]],
) -> None:
    contexts = {
        "context_invalid": [
            row for row in rows
            if row["context_valid"] == 0
        ],
        "no_pusch": [
            row for row in rows
            if row["context_valid"] == 1
            and row["ul_pusch_job_count"] == 0
        ],
        "pusch": [
            row for row in rows
            if row["context_valid"] == 1
            and row["ul_pusch_job_count"] > 0
        ],
        "decode": [
            row for row in rows
            if row["context_valid"] == 1
            and row["ul_pusch_decode_count"] > 0
        ],
        "crc_fail": [
            row for row in rows
            if row["context_valid"] == 1
            and row["ul_crc_fail_count"] > 0
        ],
        "predecode_crc": [
            row for row in rows
            if row["context_valid"] == 1
            and (
                row["ul_crc_ok_count"]
                + row["ul_crc_fail_count"]
                > row["ul_pusch_decode_count"]
            )
        ],
    }

    for name, subset in contexts.items():
        if not subset:
            lines.append(
                f"context_timing context={name} samples=0"
            )
            continue

        stats = duration_stats(subset)
        late_count = sum(
            row["late"] != 0
            for row in subset
        )

        lines.append(
            "context_timing "
            f"context={name} "
            f"samples={len(subset)} "
            f"avg_us={stats['avg_us']:.3f} "
            f"max_us={stats['max_us']} "
            f"p50_us={stats['p50_us']} "
            f"p90_us={stats['p90_us']} "
            f"p99_us={stats['p99_us']} "
            f"p999_us={stats['p999_us']} "
            f"late_count={late_count} "
            f"late_ratio_ppm={ratio_ppm(late_count, len(subset))}"
        )


def summarize(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
    duration_unit_us: Optional[int],
    duration_unit_name: str = "tti",
) -> str:
    total = len(rows)
    stats = duration_stats(rows)
    late_count = sum(
        row["late"] != 0
        for row in rows
    )
    late_thresholds = sorted(
        set(row["late_threshold_us"] for row in rows)
    )

    workload = workload_summary(rows)
    semantics = semantic_checks(rows)

    lines: List[str] = []

    lines.append("RT_DEADLINE_L1RX_CAPTURE_SUMMARY")
    lines.append("probe=L1_RX_JOB_UL")
    lines.append(f"samples={total}")
    lines.append(
        f"first_capture_index={rows[0]['capture_index']}"
    )
    lines.append(
        f"last_capture_index={rows[-1]['capture_index']}"
    )
    lines.append(
        f"first_probe_total={rows[0]['probe_total']}"
    )
    lines.append(
        f"last_probe_total={rows[-1]['probe_total']}"
    )
    lines.append(f"first_frame={rows[0]['frame']}")
    lines.append(f"first_slot={rows[0]['slot']}")
    lines.append(f"last_frame={rows[-1]['frame']}")
    lines.append(f"last_slot={rows[-1]['slot']}")

    lines.append(f"min_us={stats['min_us']}")
    lines.append(f"avg_us={stats['avg_us']:.3f}")
    lines.append(f"max_us={stats['max_us']}")
    lines.append(f"p50_us={stats['p50_us']}")
    lines.append(f"p90_us={stats['p90_us']}")
    lines.append(f"p99_us={stats['p99_us']}")
    lines.append(f"p999_us={stats['p999_us']}")
    lines.append(f"p9999_us={stats['p9999_us']}")

    lines.append(
        "late_threshold_us_values="
        + ",".join(str(x) for x in late_thresholds)
    )
    lines.append(f"late_count={late_count}")
    lines.append(
        f"late_ratio_ppm={ratio_ppm(late_count, total)}"
    )

    for threshold in threshold_summaries(rows, thresholds):
        lines.append(
            "threshold "
            f"threshold_us={threshold['threshold_us']} "
            f"count={threshold['count']} "
            f"ratio_ppm={threshold['ratio_ppm']} "
            f"burst_count={threshold['burst_count']} "
            f"longest_burst={threshold['longest_burst']}"
        )

    if duration_unit_us is not None:
        append_tti_histogram_lines(
            lines,
            rows,
            duration_unit_us,
            unit_name=duration_unit_name,
        )

    lines.append(
        "workload_samples "
        f"context_valid={workload['context_valid_samples']} "
        f"context_invalid={workload['context_invalid_samples']} "
        f"pucch_rows={workload['pucch_rows']} "
        f"pusch_rows={workload['pusch_rows']} "
        f"data_rows={workload['pusch_data_rows']} "
        f"decode_rows={workload['pusch_decode_rows']} "
        f"srs_rows={workload['srs_rows']}"
    )

    lines.append(
        "workload_totals "
        f"pucch_jobs={workload['pucch_jobs_total']} "
        f"pusch_jobs={workload['pusch_jobs_total']} "
        f"pusch_data={workload['pusch_data_total']} "
        f"pusch_decode={workload['pusch_decode_total']} "
        f"srs_jobs={workload['srs_jobs_total']} "
        f"multi_pusch_rows={workload['multi_pusch_rows']} "
        f"pusch_no_decode_rows={workload['pusch_no_decode_rows']}"
    )

    lines.append(
        "crc "
        f"ok_rows={workload['crc_ok_rows']} "
        f"fail_rows={workload['crc_fail_rows']} "
        f"ok_indications={workload['crc_ok_indications']} "
        f"fail_indications={workload['crc_fail_indications']} "
        f"predecode_rows={workload['predecode_crc_rows']} "
        f"predecode_indications={workload['predecode_crc_indications']}"
    )

    lines.append(
        "pusch_timing "
        f"late_rows={workload['late_pusch_rows']} "
        f"max_duration_us={workload['max_pusch_duration_us']}"
    )

    lines.append(
        "semantic_validation "
        f"status={semantics['validation']} "
        f"error_total={semantics['error_total']}"
    )

    for name, count in semantics["errors"].items():
        lines.append(
            f"semantic_error name={name} count={count}"
        )

    diagnostics = semantics["diagnostics"]

    lines.append(
        "semantic_diagnostic "
        f"predecode_crc_rows={diagnostics['predecode_crc_rows']} "
        f"predecode_crc_indications="
        f"{diagnostics['predecode_crc_indications']}"
    )

    append_context_timing_lines(lines, rows)

    for warning in validate_order(rows):
        lines.append(f"warning={warning}")

    return "\n".join(lines)


def write_distribution_plot(
    rows: Sequence[Dict[str, int]],
    thresholds: Sequence[int],
    output_dir: Path,
    x_max_us: Optional[int],
    kind: str,
) -> Path:
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as exc:
        raise RuntimeError(
            "matplotlib is required when plotting"
        ) from exc

    if x_max_us is not None and x_max_us <= 0:
        raise ValueError(
            f"--{kind}-x-max-us must be > 0"
        )

    durations = sorted(
        row["duration_us"]
        for row in rows
    )
    sample_count = len(durations)

    if kind == "ecdf":
        y_values = [
            (index + 1) / sample_count
            for index in range(sample_count)
        ]
        y_label = "Empirical cumulative probability"
        output_name = "l1rx_duration_ecdf.png"
    elif kind == "ccdf":
        y_values = [
            (sample_count - index) / sample_count
            for index in range(sample_count)
        ]
        y_label = "Survival probability P(duration >= x)"
        output_name = "l1rx_duration_ccdf.png"
    else:
        raise ValueError(
            f"unsupported plot kind: {kind}"
        )

    if x_max_us is not None:
        output_name = (
            f"l1rx_duration_{kind}_xmax_{x_max_us}us.png"
        )

    output_dir.mkdir(
        parents=True,
        exist_ok=True,
    )
    output_path = output_dir / output_name

    figure, axis = plt.subplots()

    axis.step(
        durations,
        y_values,
        where="post",
        label="L1_RX_JOB_UL",
    )

    for threshold_us in thresholds:
        if x_max_us is not None and threshold_us > x_max_us:
            continue

        axis.axvline(
            threshold_us,
            linestyle="--",
            linewidth=1,
            label=f"{threshold_us} us",
        )

    axis.set_xlabel("Processing duration (us)")
    axis.set_ylabel(y_label)

    if kind == "ecdf":
        axis.set_ylim(0.0, 1.0)
    else:
        axis.set_yscale("log")
        axis.set_ylim(
            bottom=max(
                1.0 / (2.0 * sample_count),
                1e-7,
            ),
            top=1.0,
        )

    if x_max_us is not None:
        axis.set_xlim(right=x_max_us)

    axis.legend()
    figure.tight_layout()
    figure.savefig(
        output_path,
        dpi=160,
    )
    plt.close(figure)

    return output_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Summarize L1_RX_JOB_UL deadline capture CSV samples."
        )
    )

    parser.add_argument(
        "csv_path",
        type=Path,
        help="Path to rt_deadline_l1rx_samples.csv",
    )

    parser.add_argument(
        "--deadline-us",
        "--threshold",
        dest="thresholds",
        type=int,
        action="append",
        default=None,
        help=(
            "Offline duration observation threshold in microseconds. "
            "Can be provided multiple times."
        ),
    )

    parser.add_argument(
        "--start-index",
        type=int,
        default=None,
        help=(
            "Keep samples whose capture_index is greater than "
            "or equal to this value."
        ),
    )

    parser.add_argument(
        "--end-index",
        type=int,
        default=None,
        help=(
            "Keep samples whose capture_index is less than "
            "or equal to this value."
        ),
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
        help=(
            "Optional upper bound for the ECDF x-axis "
            "in microseconds."
        ),
    )

    parser.add_argument(
        "--ccdf-x-max-us",
        type=int,
        default=None,
        help=(
            "Optional upper bound for the CCDF x-axis "
            "in microseconds."
        ),
    )

    duration_unit = parser.add_mutually_exclusive_group()

    duration_unit.add_argument(
        "--tti-us",
        type=int,
        default=None,
        help=(
            "Report duration histogram in TTI units using "
            "the provided TTI duration in microseconds."
        ),
    )

    duration_unit.add_argument(
        "--slot-us",
        type=int,
        default=None,
        help=(
            "Report duration histogram in slot units using "
            "the provided slot duration in microseconds."
        ),
    )

    parser.add_argument(
        "--json-summary",
        type=Path,
        default=None,
        help=(
            "Write a machine-readable JSON summary while "
            "keeping the text summary on stdout."
        ),
    )

    return parser.parse_args()


def main() -> int:
    args = parse_args()

    thresholds = (
        args.thresholds
        if args.thresholds is not None
        else [200, 500, 1000, 2000]
    )
    thresholds = sorted(set(thresholds))

    try:
        rows = load_capture(args.csv_path)

        rows = filter_rows_by_capture_index(
            rows,
            args.start_index,
            args.end_index,
        )

        duration_unit_us, duration_unit_name = (
            resolve_duration_unit(args)
        )

        print(
            summarize(
                rows,
                thresholds,
                duration_unit_us,
                duration_unit_name,
            )
        )

        if args.plot_ecdf:
            output_path = write_distribution_plot(
                rows,
                thresholds,
                args.output_dir,
                args.ecdf_x_max_us,
                "ecdf",
            )
            print(f"ecdf_plot={output_path}")

        if args.plot_ccdf:
            output_path = write_distribution_plot(
                rows,
                thresholds,
                args.output_dir,
                args.ccdf_x_max_us,
                "ccdf",
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
        print(
            f"ERROR: {exc}",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
