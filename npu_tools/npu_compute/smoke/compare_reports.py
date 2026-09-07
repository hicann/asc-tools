#!/usr/bin/env python3
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

"""Produce a report-only comparison of npu-compute and msopprof CSV output."""

import argparse
import csv
import math
from pathlib import Path
from typing import Iterable


KEY_FIELDS = ("block_id", "sub_block_id")
COMPARISON_HEADER = (
    "section",
    "block_id",
    "sub_block_id",
    "field",
    "npu_compute",
    "msopprof",
    "absolute_delta",
    "relative_delta_percent",
    "status",
)
COVERAGE_HEADER = ("section", "status", "detail")


def read_csv(path: Path) -> tuple[list[str], dict[tuple[str, str], dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None:
            raise ValueError(f"CSV has no header: {path}")
        header = [field.strip() for field in reader.fieldnames]
        while header and not header[-1]:
            header.pop()
        if any(not field for field in header):
            raise ValueError(f"CSV has an empty header field: {path}")
        if not all(field in header for field in KEY_FIELDS):
            raise ValueError(f"CSV has no comparison keys: {path}")
        rows: dict[tuple[str, str], dict[str, str]] = {}
        for row in reader:
            key = tuple((row.get(field) or "").strip() for field in KEY_FIELDS)
            if not all(key):
                continue
            if key in rows:
                raise ValueError(
                    f"CSV contains a duplicate comparison key {key}: {path}"
                )
            rows[key] = {field: (row.get(field) or "").strip() for field in header}
    return header, rows


def candidate_csv_files(root: Path) -> list[Path]:
    if not root.is_dir():
        return []
    candidates: list[Path] = []
    for path in sorted(root.rglob("*.csv")):
        try:
            header, _ = read_csv(path)
        except (OSError, ValueError):
            continue
        if all(field in header for field in KEY_FIELDS):
            candidates.append(path)
    return candidates


def find_npu_compute_csv(root: Path, section: str) -> Path | None:
    direct = root / f"{section}.csv"
    if direct.is_file():
        return direct
    candidates = [
        path for path in candidate_csv_files(root) if path.name == f"{section}.csv"
    ]
    return candidates[0] if len(candidates) == 1 else None


def find_msopprof_csv(
    root: Path, section: str, npu_header: Iterable[str]
) -> Path | None:
    section_root = root / section
    candidates = candidate_csv_files(section_root)
    if not candidates:
        return None

    npu_fields = set(npu_header)
    return max(
        candidates,
        key=lambda path: (
            len(set(read_csv(path)[0]) & npu_fields),
            -len(str(path)),
            str(path),
        ),
    )


def numeric_value(value: str) -> float | None:
    if not value or value.upper() in {"NA", "N/A", "NAN", "INF", "-INF"}:
        return None
    try:
        parsed = float(value)
    except ValueError:
        return None
    return parsed if math.isfinite(parsed) else None


def format_number(value: float) -> str:
    return f"{value:.12g}"


def compare_section(
    section: str, npu_path: Path | None, msopprof_path: Path | None
) -> tuple[list[dict[str, str]], dict[str, str]]:
    if npu_path is None and msopprof_path is None:
        return [], {"section": section, "status": "missing-both", "detail": ""}
    if npu_path is None:
        return [], {
            "section": section,
            "status": "missing-npu-compute",
            "detail": str(msopprof_path),
        }
    if msopprof_path is None:
        return [], {
            "section": section,
            "status": "missing-msopprof",
            "detail": str(npu_path),
        }

    npu_header, npu_rows = read_csv(npu_path)
    msopprof_header, msopprof_rows = read_csv(msopprof_path)
    fields = sorted((set(npu_header) | set(msopprof_header)) - set(KEY_FIELDS))
    results: list[dict[str, str]] = []
    for key in sorted(set(npu_rows) | set(msopprof_rows)):
        npu_row = npu_rows.get(key)
        msopprof_row = msopprof_rows.get(key)
        for field in fields:
            result = {
                "section": section,
                "block_id": key[0],
                "sub_block_id": key[1],
                "field": field,
                "npu_compute": "" if npu_row is None else npu_row.get(field, ""),
                "msopprof": "" if msopprof_row is None else msopprof_row.get(field, ""),
                "absolute_delta": "",
                "relative_delta_percent": "",
                "status": "",
            }
            if npu_row is None:
                result["status"] = "missing-npu-compute-row"
            elif msopprof_row is None:
                result["status"] = "missing-msopprof-row"
            elif field not in npu_header:
                result["status"] = "missing-npu-compute-field"
            elif field not in msopprof_header:
                result["status"] = "missing-msopprof-field"
            else:
                npu_value = numeric_value(result["npu_compute"])
                msopprof_value = numeric_value(result["msopprof"])
                if npu_value is None or msopprof_value is None:
                    result["status"] = "not-comparable"
                else:
                    absolute_delta = abs(msopprof_value - npu_value)
                    result["absolute_delta"] = format_number(absolute_delta)
                    if npu_value != 0:
                        result["relative_delta_percent"] = (
                            f"{absolute_delta / abs(npu_value) * 100:.6f}"
                        )
                    result["status"] = "compared"
            results.append(result)
    return results, {
        "section": section,
        "status": "compared",
        "detail": f"{npu_path}|{msopprof_path}",
    }


def write_csv(path: Path, header: tuple[str, ...], rows: list[dict[str, str]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=header)
        writer.writeheader()
        writer.writerows(rows)


def write_summary(
    path: Path, coverage: list[dict[str, str]], comparison: list[dict[str, str]]
) -> None:
    compared = sum(row["status"] == "compared" for row in comparison)
    missing = sum(row["status"].startswith("missing-") for row in comparison)
    with path.open("w", encoding="utf-8") as stream:
        stream.write("# npu-compute and msopprof Comparison\n\n")
        stream.write(
            "This is a report-only comparison. Metric differences do not change the command exit status.\n\n"
        )
        stream.write("| Section | Collection status | Detail |\n| --- | --- | --- |\n")
        for row in coverage:
            stream.write(f"| {row['section']} | {row['status']} | {row['detail']} |\n")
        stream.write(f"\nCompared numeric values: {compared}\n\n")
        stream.write(f"Rows or fields missing from one tool: {missing}\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--npu-root", required=True, type=Path)
    parser.add_argument("--msopprof-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--section", action="append", required=True)
    args = parser.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    comparison: list[dict[str, str]] = []
    coverage: list[dict[str, str]] = []
    for section in args.section:
        npu_path = find_npu_compute_csv(args.npu_root, section)
        npu_header = read_csv(npu_path)[0] if npu_path is not None else []
        msopprof_path = find_msopprof_csv(args.msopprof_root, section, npu_header)
        section_rows, section_coverage = compare_section(
            section, npu_path, msopprof_path
        )
        comparison.extend(section_rows)
        coverage.append(section_coverage)

    write_csv(args.output / "comparison.csv", COMPARISON_HEADER, comparison)
    write_csv(args.output / "coverage.csv", COVERAGE_HEADER, coverage)
    write_summary(args.output / "summary.md", coverage, comparison)
    print(f"comparison report: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
