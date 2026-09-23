#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

"""用途：补充逐元素容差、数值精确或位模式诊断及错误位置；不替代不同判据的项目验收。
使用方法（在 Skill 根目录执行）：
    python3 scripts/analysis/precision_compare.py --help

参数和示例见 scripts/usage/precision-compare.md。
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import math
import os
from pathlib import Path
import tempfile
from typing import Any
import sys

try:
    import numpy as np
except ModuleNotFoundError as exc:
    if exc.name != "numpy":
        raise
    np = None

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


SCHEMA_VERSION = "1.0.0"
BF16_NAMES = {"bf16", "bfloat16", "bfp16"}
DTYPE_ALIASES = (
    {
        "bool": np.bool_,
        "fp16": np.float16,
        "float16": np.float16,
        "half": np.float16,
        "fp32": np.float32,
        "float32": np.float32,
        "float": np.float32,
        "fp64": np.float64,
        "float64": np.float64,
        "double": np.float64,
        "int8": np.int8,
        "int16": np.int16,
        "int32": np.int32,
        "int64": np.int64,
        "uint8": np.uint8,
        "uint16": np.uint16,
        "uint32": np.uint32,
        "uint64": np.uint64,
    }
    if np is not None
    else {}
)
UNSIGNED_DTYPES_BY_BYTES = (
    {
        1: np.dtype(np.uint8),
        2: np.dtype(np.uint16),
        4: np.dtype(np.uint32),
        8: np.dtype(np.uint64),
    }
    if np is not None
    else {}
)
FLOAT_FRACTION_BITS_BY_BYTES = {2: 10, 4: 23, 8: 52}


class ComparisonInputError(ValueError):
    """A user-correctable tensor or comparison contract error."""


@dataclass(frozen=True)
class LoadedTensor:
    """Keep numerical values separate from their original encoding."""

    values: np.ndarray[Any, Any]
    storage: np.ndarray[Any, Any]
    dtype: str


def parse_shape(value: str | None) -> tuple[int, ...] | None:
    if value is None:
        return None
    pieces = [piece.strip() for piece in value.split(",")]
    if not pieces or any(not piece for piece in pieces):
        raise ComparisonInputError(f"invalid shape: {value!r}")
    try:
        shape = tuple(int(piece) for piece in pieces)
    except ValueError as exc:
        raise ComparisonInputError(f"invalid shape: {value!r}") from exc
    if any(dimension <= 0 for dimension in shape):
        raise ComparisonInputError("shape dimensions must be positive")
    return shape


def normalize_dtype(value: str | None) -> str | None:
    return value.strip().lower() if value else None


def numpy_dtype(value: str) -> np.dtype[Any]:
    normalized = normalize_dtype(value)
    if normalized in BF16_NAMES:
        return np.dtype(np.uint16)
    dtype = DTYPE_ALIASES.get(str(normalized))
    if dtype is None:
        raise ComparisonInputError(f"unsupported dtype: {value}")
    return np.dtype(dtype)


def bf16_uint16_to_fp32(raw: np.ndarray[Any, Any]) -> np.ndarray[Any, Any]:
    contiguous = np.ascontiguousarray(raw, dtype=np.uint16)
    return (contiguous.astype(np.uint32) << 16).view(np.float32).reshape(raw.shape)


def ensure_supported_tensor(array: np.ndarray[Any, Any], label: str) -> None:
    if array.size == 0:
        raise ComparisonInputError(f"{label} tensor is empty")
    if array.dtype.kind not in {"b", "i", "u", "f"}:
        raise ComparisonInputError(
            f"{label} dtype {array.dtype} is unsupported; use bool, integer or real float"
        )


def load_tensor(
    path_value: str,
    dtype_value: str | None,
    shape_value: str | None,
    label: str,
) -> LoadedTensor:
    path = Path(path_value)
    if not path.is_file():
        raise ComparisonInputError(f"{label} file not found: {path}")
    dtype_name = normalize_dtype(dtype_value)
    shape = parse_shape(shape_value)
    suffix = path.suffix.lower()

    if suffix == ".npy":
        try:
            array = np.load(path, allow_pickle=False)
        except (OSError, ValueError) as exc:
            raise ComparisonInputError(f"cannot load {label} .npy: {exc}") from exc
        if not isinstance(array, np.ndarray):
            raise ComparisonInputError(f"{label} .npy does not contain an ndarray")
        if dtype_name in BF16_NAMES:
            if array.dtype != np.uint16:
                raise ComparisonInputError(
                    f"{label} BF16 .npy storage must be uint16, got {array.dtype}"
                )
        elif dtype_name is not None and array.dtype != numpy_dtype(dtype_name):
            raise ComparisonInputError(
                f"{label} .npy dtype mismatch: file={array.dtype} requested={dtype_name}"
            )
        if shape is not None and tuple(array.shape) != shape:
            raise ComparisonInputError(
                f"{label} .npy shape mismatch: file={tuple(array.shape)} requested={shape}"
            )
    elif suffix == ".bin":
        if dtype_name is None or shape is None:
            raise ComparisonInputError(
                f"{label} .bin requires an explicit dtype and shape"
            )
        storage_dtype = numpy_dtype(dtype_name)
        expected_elements = math.prod(shape)
        expected_bytes = expected_elements * storage_dtype.itemsize
        actual_bytes = path.stat().st_size
        if actual_bytes != expected_bytes:
            raise ComparisonInputError(
                f"{label} .bin size mismatch: expected={expected_bytes} actual={actual_bytes}"
            )
        array = np.fromfile(path, dtype=storage_dtype).reshape(shape)
    else:
        raise ComparisonInputError(
            f"unsupported {label} format {suffix!r}; use .npy or .bin"
        )

    ensure_supported_tensor(array, label)
    if dtype_name in BF16_NAMES:
        return LoadedTensor(bf16_uint16_to_fp32(array), array, "bfloat16")
    return LoadedTensor(array, array, str(array.dtype))


def load_selection(
    shape: tuple[int, ...],
    valid_count: int | None,
    mask_path: str | None,
) -> tuple[np.ndarray[Any, np.dtype[np.bool_]], str]:
    if valid_count is not None and mask_path is not None:
        raise ComparisonInputError("--valid-count and --mask are mutually exclusive")
    total = math.prod(shape)
    if valid_count is not None:
        if valid_count <= 0 or valid_count > total:
            raise ComparisonInputError(
                f"--valid-count must be in [1, {total}], got {valid_count}"
            )
        selection = np.zeros(total, dtype=np.bool_)
        selection[:valid_count] = True
        return selection.reshape(shape), "flat-prefix"
    if mask_path is not None:
        path = Path(mask_path)
        if not path.is_file() or path.suffix.lower() != ".npy":
            raise ComparisonInputError("--mask must name an existing boolean .npy file")
        try:
            mask = np.load(path, allow_pickle=False)
        except (OSError, ValueError) as exc:
            raise ComparisonInputError(f"cannot load mask: {exc}") from exc
        if not isinstance(mask, np.ndarray) or mask.dtype != np.bool_:
            raise ComparisonInputError("mask must be a boolean ndarray")
        if tuple(mask.shape) != shape:
            raise ComparisonInputError(
                f"mask shape mismatch: mask={tuple(mask.shape)} tensor={shape}"
            )
        if not bool(np.any(mask)):
            raise ComparisonInputError("mask selects no elements")
        return mask, "boolean-mask"
    return np.ones(shape, dtype=np.bool_), "all"


def json_scalar(value: Any) -> bool | int | float | str:
    scalar = value.item() if isinstance(value, np.generic) else value
    if isinstance(scalar, (bool, np.bool_)):
        return bool(scalar)
    if isinstance(scalar, (int, np.integer)):
        return int(scalar)
    number = float(scalar)
    if math.isnan(number):
        return "NaN"
    if math.isinf(number):
        return "+Inf" if number > 0 else "-Inf"
    return number


def optional_max(values: np.ndarray[Any, Any]) -> float | None:
    return float(np.max(values)) if values.size else None


def optional_mean(values: np.ndarray[Any, Any]) -> float | None:
    if not values.size:
        return None
    scale = float(np.max(np.abs(values)))
    if not math.isfinite(scale) or scale == 0:
        return scale
    return float(np.mean(values / scale)) * scale


def root_mean_square(values: np.ndarray[Any, Any]) -> float | None:
    if not values.size:
        return None
    scale = float(np.max(np.abs(values)))
    if not math.isfinite(scale) or scale == 0:
        return scale
    return float(np.sqrt(np.mean(np.square(values / scale)))) * scale


def raw_bit_patterns(
    values: np.ndarray[Any, Any],
) -> np.ndarray[Any, np.dtype[np.uint64]]:
    unsigned_dtype = UNSIGNED_DTYPES_BY_BYTES.get(values.dtype.itemsize)
    if unsigned_dtype is None:
        raise ComparisonInputError(
            f"bitwise mode does not support {values.dtype.itemsize}-byte dtype {values.dtype}"
        )
    if values.dtype.byteorder in {"<", ">"}:
        unsigned_dtype = unsigned_dtype.newbyteorder(values.dtype.byteorder)
    contiguous = np.ascontiguousarray(values)
    return contiguous.view(unsigned_dtype).reshape(contiguous.shape).astype(np.uint64)


def float_bit_masks(
    dtype: np.dtype[Any], *, bfloat16: bool = False
) -> tuple[np.uint64, np.uint64]:
    fraction_bits = 7 if bfloat16 else FLOAT_FRACTION_BITS_BY_BYTES.get(dtype.itemsize)
    if fraction_bits is None:
        raise ComparisonInputError(
            f"bitwise mode does not support floating dtype {dtype}"
        )
    sign_mask = np.uint64(1 << (dtype.itemsize * 8 - 1))
    payload_mask = np.uint64((1 << fraction_bits) - 1)
    return sign_mask, payload_mask


def compare_tensors(
    actual_tensor: LoadedTensor,
    golden_tensor: LoadedTensor,
    *,
    rtol: float,
    atol: float,
    mode: str,
    selection: np.ndarray[Any, np.dtype[np.bool_]],
    top_k: int,
) -> dict[str, Any]:
    actual, golden = actual_tensor.values, golden_tensor.values
    if actual.shape != golden.shape:
        raise ComparisonInputError(
            f"shape mismatch: actual={actual.shape} golden={golden.shape}"
        )
    if selection.shape != actual.shape or selection.dtype != np.bool_:
        raise ComparisonInputError("internal selection contract mismatch")
    if not bool(np.any(selection)):
        raise ComparisonInputError("selection contains no valid elements")

    actual_values = actual[selection]
    golden_values = golden[selection]
    if mode == "bitwise" and (
        actual_tensor.dtype != golden_tensor.dtype
        or actual_tensor.storage.dtype != golden_tensor.storage.dtype
    ):
        raise ComparisonInputError(
            "bitwise mode requires the same dtype: "
            f"actual={actual_tensor.dtype} golden={golden_tensor.dtype}"
        )
    with np.errstate(over="ignore", invalid="ignore"):
        actual64 = actual_values.astype(np.float64)
        golden64 = golden_values.astype(np.float64)

    actual_nan = np.isnan(actual64)
    golden_nan = np.isnan(golden64)
    actual_inf = np.isinf(actual64)
    golden_inf = np.isinf(golden64)
    finite = np.isfinite(actual64) & np.isfinite(golden64)
    numerically_matched_nan = actual_nan & golden_nan
    numerically_matched_inf = (
        actual_inf & golden_inf & (np.signbit(actual64) == np.signbit(golden64))
    )

    integer_pair = actual.dtype.kind in {"b", "i", "u"} and golden.dtype.kind in {
        "b",
        "i",
        "u",
    }
    effective_mode = (
        "bitwise"
        if mode == "bitwise"
        else "exact"
        if mode == "exact" or (mode == "auto" and integer_pair)
        else "tolerance"
    )
    finite_mismatch = np.zeros(actual_values.shape, dtype=np.bool_)
    with np.errstate(over="ignore", invalid="ignore"):
        finite_delta = actual64[finite] - golden64[finite]
        finite_abs_error = np.abs(finite_delta)
    bitwise_stats: dict[str, int] | None = None
    actual_bits: np.ndarray[Any, np.dtype[np.uint64]] | None = None
    golden_bits: np.ndarray[Any, np.dtype[np.uint64]] | None = None
    signed_zero_mismatch = np.zeros(actual_values.shape, dtype=np.bool_)
    nan_payload_mismatch = np.zeros(actual_values.shape, dtype=np.bool_)
    nan_sign_mismatch = np.zeros(actual_values.shape, dtype=np.bool_)
    numeric_bit_mismatch = np.zeros(actual_values.shape, dtype=np.bool_)
    if effective_mode == "bitwise":
        actual_bits = raw_bit_patterns(actual_tensor.storage[selection])
        golden_bits = raw_bit_patterns(golden_tensor.storage[selection])
        mismatch = np.not_equal(actual_bits, golden_bits)
        finite_mismatch[finite] = mismatch[finite]
        matched_nan = numerically_matched_nan & ~mismatch
        matched_inf = numerically_matched_inf & ~mismatch
        special_mismatch = mismatch & ~finite
        if actual.dtype.kind == "f":
            sign_mask, payload_mask = float_bit_masks(
                actual_tensor.storage.dtype, bfloat16=actual_tensor.dtype == "bfloat16"
            )
            both_zero = finite & (actual64 == 0.0) & (golden64 == 0.0)
            signed_zero_mismatch = mismatch & both_zero
            nan_sign_mismatch = numerically_matched_nan & (
                (actual_bits & sign_mask) != (golden_bits & sign_mask)
            )
            nan_payload_mismatch = numerically_matched_nan & (
                (actual_bits & payload_mask) != (golden_bits & payload_mask)
            )
            numeric_bit_mismatch = mismatch & finite & ~both_zero
        else:
            numeric_bit_mismatch = mismatch
        bitwise_stats = {
            "bitwise_mismatch_count": int(np.sum(mismatch)),
            "signed_zero_mismatch_count": int(np.sum(signed_zero_mismatch)),
            "nan_payload_mismatch_count": int(np.sum(nan_payload_mismatch)),
            "nan_sign_mismatch_count": int(np.sum(nan_sign_mismatch)),
            "numeric_bit_mismatch_count": int(np.sum(numeric_bit_mismatch)),
        }
    elif effective_mode == "exact":
        matched_nan = numerically_matched_nan
        matched_inf = numerically_matched_inf
        special_mismatch = ~(finite | matched_nan | matched_inf)
        left, right = actual_values[finite], golden_values[finite]
        if (actual.dtype.kind == "f") != (golden.dtype.kind == "f"):
            # Python int/float equality preserves the integer's low bits;
            # NumPy's common floating dtype may round them away before comparing.
            left, right = left.astype(object), right.astype(object)
        finite_mismatch[finite] = np.not_equal(left, right)
        mismatch = special_mismatch | finite_mismatch
    else:
        matched_nan = numerically_matched_nan
        matched_inf = numerically_matched_inf
        special_mismatch = ~(finite | matched_nan | matched_inf)
        with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
            finite_threshold = atol + rtol * np.abs(golden64[finite])
            failed = finite_abs_error > finite_threshold
            overflow = ~np.isfinite(finite_abs_error) | ~np.isfinite(finite_threshold)
            if np.any(overflow):
                left, right = actual64[finite][overflow], golden64[finite][overflow]
                scale = np.maximum(np.abs(left), np.abs(right))
                failed[overflow] = np.abs(left / scale - right / scale) > (
                    atol / scale + rtol * (np.abs(right) / scale)
                )
        finite_mismatch[finite] = failed
        mismatch = special_mismatch | finite_mismatch

    nonzero_reference = finite & (golden64 != 0.0)
    with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
        relative_error = np.abs(
            actual64[nonzero_reference] - golden64[nonzero_reference]
        ) / np.abs(golden64[nonzero_reference])
    zero_reference_nonzero = finite & (golden64 == 0.0) & (actual64 != 0.0)
    cosine = None
    if bool(np.any(finite)):
        left, right = actual64[finite], golden64[finite]
        left_scale, right_scale = np.max(np.abs(left)), np.max(np.abs(right))
        if left_scale > 0 and right_scale > 0:
            left, right = left / left_scale, right / right_scale
            cosine = float(
                np.dot(left, right) / (np.linalg.norm(left) * np.linalg.norm(right))
            )

    selected_flat_indices = np.flatnonzero(selection.reshape(-1))
    mismatch_local_indices = np.flatnonzero(mismatch)
    mismatch_full_indices = selected_flat_indices[mismatch_local_indices].astype(
        np.int64, copy=False
    )
    mismatch_index_sha256 = hashlib.sha256(
        mismatch_full_indices.astype("<i8", copy=False).tobytes()
    ).hexdigest()
    ranked: list[tuple[int, float, int, int]] = []
    for local_index in mismatch_local_indices:
        bitwise_special = effective_mode == "bitwise" and bool(
            signed_zero_mismatch[local_index]
            or nan_payload_mismatch[local_index]
            or nan_sign_mismatch[local_index]
        )
        special_rank = 1 if special_mismatch[local_index] or bitwise_special else 0
        magnitude = (
            math.inf
            if special_rank
            else abs(float(actual64[local_index]) - float(golden64[local_index]))
        )
        full_index = int(selected_flat_indices[local_index])
        ranked.append((special_rank, magnitude, full_index, int(local_index)))
    ranked.sort(key=lambda item: (-item[0], -item[1], item[2]))

    top_mismatches: list[dict[str, Any]] = []
    for special_rank, magnitude, full_index, local_index in ranked[:top_k]:
        if effective_mode == "bitwise":
            has_nan_sign = bool(nan_sign_mismatch[local_index])
            has_nan_payload = bool(nan_payload_mismatch[local_index])
            if has_nan_sign and has_nan_payload:
                mismatch_kind = "nan-sign-and-payload"
            elif has_nan_sign:
                mismatch_kind = "nan-sign"
            elif has_nan_payload:
                mismatch_kind = "nan-payload"
            elif bool(signed_zero_mismatch[local_index]):
                mismatch_kind = "signed-zero"
            elif bool(numeric_bit_mismatch[local_index]):
                mismatch_kind = "numeric-bits"
            else:
                mismatch_kind = "special-value"
        else:
            mismatch_kind = "special-value" if special_rank else "finite-value"
        item = {
            "flat_index": full_index,
            "position": [
                int(value) for value in np.unravel_index(full_index, actual.shape)
            ],
            "kind": mismatch_kind,
            "actual": json_scalar(actual_values[local_index]),
            "golden": json_scalar(golden_values[local_index]),
            "absolute_error": None if not math.isfinite(magnitude) else magnitude,
        }
        if actual_bits is not None and golden_bits is not None:
            width = actual_tensor.storage.dtype.itemsize * 2
            item["actual_bits"] = f"0x{int(actual_bits[local_index]):0{width}x}"
            item["golden_bits"] = f"0x{int(golden_bits[local_index]):0{width}x}"
        top_mismatches.append(item)

    compared = int(actual_values.size)
    error_elements = int(np.sum(mismatch))
    metrics = {
        "passed": error_elements == 0,
        "effective_mode": effective_mode,
        "compared_elements": compared,
        "error_elements": error_elements,
        "error_ratio": error_elements / compared,
        "first_error_position": (
            [
                int(value)
                for value in np.unravel_index(
                    int(mismatch_full_indices[0]), actual.shape
                )
            ]
            if mismatch_full_indices.size
            else None
        ),
        "mismatch_index_sha256": mismatch_index_sha256,
        "finite_pair_count": int(np.sum(finite)),
        "finite_mismatch_count": int(np.sum(finite_mismatch)),
        "special_value_mismatch_count": int(np.sum(special_mismatch)),
        "matched_nan_count": int(np.sum(matched_nan)),
        "matched_inf_count": int(np.sum(matched_inf)),
        "actual_nan_count": int(np.sum(actual_nan)),
        "golden_nan_count": int(np.sum(golden_nan)),
        "actual_inf_count": int(np.sum(actual_inf)),
        "golden_inf_count": int(np.sum(golden_inf)),
        "max_abs_error": optional_max(finite_abs_error),
        "mean_abs_error": optional_mean(finite_abs_error),
        "rmse": root_mean_square(finite_delta),
        "max_rel_error": optional_max(relative_error),
        "mean_rel_error": optional_mean(relative_error),
        "relative_error_defined_count": int(relative_error.size),
        "zero_reference_nonzero_count": int(np.sum(zero_reference_nonzero)),
        "cosine_similarity": cosine,
        "top_error_positions": [item["position"] for item in top_mismatches],
        "top_mismatches": top_mismatches,
    }
    if bitwise_stats is not None:
        metrics.update(bitwise_stats)
    nonfinite = [
        key
        for key, value in metrics.items()
        if isinstance(value, float) and not math.isfinite(value)
    ]
    for key in nonfinite:
        metrics[key] = None
    metrics["nonfinite_metric_fields"] = nonfinite
    return metrics


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--actual", "--npu", dest="actual", required=True)
    parser.add_argument("--golden", required=True)
    parser.add_argument("--dtype", help="common dtype for .bin inputs")
    parser.add_argument("--actual-dtype", "--npu-dtype", dest="actual_dtype")
    parser.add_argument("--golden-dtype")
    parser.add_argument("--shape", help="common shape for .bin inputs, e.g. 4,8")
    parser.add_argument("--actual-shape", "--npu-shape", dest="actual_shape")
    parser.add_argument("--golden-shape")
    parser.add_argument(
        "--mode", choices=("auto", "exact", "tolerance", "bitwise"), default="auto"
    )
    parser.add_argument("--rtol", type=float, default=1e-3)
    parser.add_argument("--atol", type=float, default=1e-5)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--valid-count", type=int)
    selection.add_argument(
        "--mask", help="boolean .npy mask selecting logical valid elements"
    )
    parser.add_argument("--top-k", type=int, default=10)
    parser.add_argument("--output", help="JSON report path; default stdout")
    return parser


def validate_options(args: argparse.Namespace) -> None:
    for name in ("rtol", "atol"):
        value = getattr(args, name)
        if not math.isfinite(value) or value < 0:
            raise ComparisonInputError(f"--{name} must be a finite non-negative value")
    if args.top_k < 0 or args.top_k > 1000:
        raise ComparisonInputError("--top-k must be in [0, 1000]")
    if args.output:
        output = Path(args.output).resolve()
        protected_inputs = {
            Path(value).resolve()
            for value in (args.actual, args.golden, args.mask)
            if value is not None
        }
        if output in protected_inputs:
            raise ComparisonInputError(
                "--output must not overwrite the actual, golden or mask input"
            )


def execute(args: argparse.Namespace) -> dict[str, Any]:
    if np is None:
        raise ComparisonInputError(
            "NumPy is required for tensor comparison; install it in the Python environment used to run this tool"
        )
    validate_options(args)
    actual_dtype = args.actual_dtype or args.dtype
    golden_dtype = args.golden_dtype or args.dtype
    actual_shape = args.actual_shape or args.shape
    golden_shape = args.golden_shape or args.shape
    actual = load_tensor(args.actual, actual_dtype, actual_shape, "actual")
    golden = load_tensor(args.golden, golden_dtype, golden_shape, "golden")
    if actual.values.shape != golden.values.shape:
        raise ComparisonInputError(
            f"shape mismatch: actual={actual.values.shape} golden={golden.values.shape}"
        )
    selection, selection_mode = load_selection(
        actual.values.shape, args.valid_count, args.mask
    )
    metrics = compare_tensors(
        actual,
        golden,
        rtol=args.rtol,
        atol=args.atol,
        mode=args.mode,
        selection=selection,
        top_k=args.top_k,
    )
    input_elements = int(actual.values.size)
    report = {
        "schema_version": SCHEMA_VERSION,
        "status": "PASS" if metrics["passed"] else "MISMATCH",
        "actual": {
            "path": str(Path(args.actual).resolve()),
            "shape": list(actual.values.shape),
            "dtype": actual.dtype,
            "storage_dtype": str(actual.storage.dtype),
        },
        "golden": {
            "path": str(Path(args.golden).resolve()),
            "shape": list(golden.values.shape),
            "dtype": golden.dtype,
            "storage_dtype": str(golden.storage.dtype),
        },
        "selection": {
            "mode": selection_mode,
            "input_elements": input_elements,
            "compared_elements": metrics["compared_elements"],
            "excluded_elements": input_elements - metrics["compared_elements"],
            "mask": str(Path(args.mask).resolve()) if args.mask else None,
        },
        "requested_mode": args.mode,
        "rtol": args.rtol,
        "atol": args.atol,
        "metrics": metrics,
        "interpretation": (
            "This is an auxiliary elementwise comparison; the task's trusted evaluator remains authoritative. "
            "Undefined or non-finite auxiliary metrics are null; nonfinite_metric_fields identifies range limits."
        ),
    }
    return report


def main() -> int:
    args = build_parser().parse_args()
    try:
        report = execute(args)
        rendered = (
            json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
        )
        if args.output:
            atomic_write(Path(args.output), rendered)
        else:
            print(rendered, end="")
        return 0 if report["status"] == "PASS" else 1
    except (ComparisonInputError, OSError, ValueError, TypeError) as exc:
        print(
            json.dumps(
                {
                    "schema_version": SCHEMA_VERSION,
                    "status": "INVALID",
                    "error": str(exc),
                },
                ensure_ascii=False,
                indent=2,
            ),
            file=sys.stderr,
        )
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
