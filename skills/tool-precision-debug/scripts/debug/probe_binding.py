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

"""用途：校验工具与能力探测报告的绑定关系。
使用方法：由同目录调试脚本导入；无独立命令行。
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any


PROBE_SCHEMA_VERSION = "1.1.0"
CAPABILITIES = {
    "cpu-debug",
    "npu-check",
    "msobjdump",
    "show-kernel-debug-data",
}
MAX_PROBE_BYTES = 4 * 1024 * 1024


class ProbeBindingError(ValueError):
    """The probe report cannot support the requested execution binding."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _load_report(path_value: str | Path) -> tuple[Path, dict[str, Any]]:
    path = Path(path_value).expanduser()
    if path.is_symlink():
        raise ProbeBindingError(f"symbolic-link probe report is not allowed: {path}")
    try:
        resolved = path.resolve(strict=True)
    except OSError as exc:
        raise ProbeBindingError(f"cannot resolve probe report: {exc}") from exc
    if not resolved.is_file():
        raise ProbeBindingError(f"probe report is not a regular file: {resolved}")
    size = resolved.stat().st_size
    if size <= 0 or size > MAX_PROBE_BYTES:
        raise ProbeBindingError(
            f"probe report size must be in [1, {MAX_PROBE_BYTES}], got {size}"
        )
    try:
        report = json.loads(resolved.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ProbeBindingError(f"cannot load probe report JSON: {exc}") from exc
    if not isinstance(report, dict):
        raise ProbeBindingError("probe report must be a JSON object")
    return resolved, report


def bind_probe_report(
    path_value: str | Path,
    *,
    expected_cann: str,
    npu_arch: str,
    required_capabilities: list[str],
) -> dict[str, Any]:
    requested_capabilities = list(dict.fromkeys(required_capabilities))
    unknown = sorted(set(requested_capabilities) - CAPABILITIES)
    if unknown:
        raise ProbeBindingError(f"unsupported requested capabilities: {unknown}")
    if not requested_capabilities:
        raise ProbeBindingError("at least one capability must be requested")

    path, report = _load_report(path_value)
    if (
        report.get("schema_version") != PROBE_SCHEMA_VERSION
        or report.get("mode") != "probe"
    ):
        raise ProbeBindingError(
            f"probe report must use asc-tools probe schema {PROBE_SCHEMA_VERSION}"
        )
    if report.get("expected_cann") != expected_cann:
        raise ProbeBindingError(
            "probe report is not bound to the declared CANN version"
        )
    if str(report.get("npu_arch")) != str(npu_arch):
        raise ProbeBindingError(
            "probe report is not bound to the declared NPU architecture"
        )

    requirements = report.get("requirements")
    requested = (
        requirements.get("requested") if isinstance(requirements, dict) else None
    )
    if (
        not isinstance(requested, list)
        or requirements.get("satisfied") is not True
        or not set(requested_capabilities).issubset(set(requested))
    ):
        raise ProbeBindingError(
            "probe report must explicitly satisfy every requested capability"
        )

    selected_installation: dict[str, Any] | None = None
    selected_capabilities: dict[str, dict[str, Any]] = {}
    for installation in report.get("installations", []):
        if not isinstance(installation, dict):
            continue
        version = installation.get("version")
        if (
            not isinstance(version, dict)
            or version.get("value") != expected_cann
            or version.get("match") != "match"
        ):
            continue
        capabilities = {
            item.get("id"): item
            for item in installation.get("capabilities", [])
            if isinstance(item, dict) and isinstance(item.get("id"), str)
        }
        if all(
            capability in capabilities
            and capabilities[capability].get("status") == "available"
            for capability in requested_capabilities
        ):
            selected_installation = installation
            selected_capabilities = {
                capability: capabilities[capability]
                for capability in requested_capabilities
            }
            break
    if selected_installation is None:
        raise ProbeBindingError(
            "no single matching CANN installation provides all requested capabilities"
        )

    capabilities_record = []
    for capability_id in requested_capabilities:
        capability = selected_capabilities[capability_id]
        target_probe = capability.get("target_probe")
        if capability_id in {"cpu-debug", "npu-check"}:
            if (
                not isinstance(target_probe, dict)
                or target_probe.get("attempted") is not True
                or target_probe.get("returncode") != 0
                or str(target_probe.get("npu_arch")) != str(npu_arch)
            ):
                raise ProbeBindingError(
                    f"{capability_id} lacks a successful target probe for NPU architecture {npu_arch}"
                )
        capabilities_record.append(
            {
                "id": capability_id,
                "status": capability.get("status"),
                "entrypoint": capability.get("entrypoint"),
                "cmake_target": (
                    target_probe.get("cmake_target")
                    if isinstance(target_probe, dict)
                    else None
                ),
            }
        )

    return {
        "path": str(path),
        "size_bytes": path.stat().st_size,
        "sha256": sha256_file(path),
        "expected_cann": expected_cann,
        "npu_arch": str(npu_arch),
        "installation_root": selected_installation.get("root"),
        "capabilities": capabilities_record,
    }
