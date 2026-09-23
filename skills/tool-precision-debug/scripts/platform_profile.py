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

"""用途：识别 CANN、产品、架构与编程模型的共享模块。
使用方法：由 scripts/detect_profile.py 和 scripts/debug/asc_tools_probe.py 导入；无独立命令行。
"""

from __future__ import annotations

import os
import json
from pathlib import Path
import re
import shutil
import subprocess
from dataclasses import dataclass
from typing import Any

DIMENSIONS = ("cann", "soc", "npu_arch", "programming_model")
INVOCATION_MODELS = frozenset({"kernel-direct"})
EXECUTION_PATHS = frozenset(
    {
        "tensor-api",
        "regbase",
        "simd",
        "simt",
        "simd-simt-mixed",
    }
)


def normalize_cann(value: str) -> str:
    match = re.search(r"\d+\.\d+\.\d+", value)
    return match.group(0) if match else value.strip().lower()


def normalize_soc(value: str) -> str:
    return re.sub(r"[^a-z0-9]", "", value.lower())


def normalize_arch(value: str) -> str:
    lowered = value.lower().replace("dav", "")
    return re.sub(r"[^0-9a-z]", "", lowered)


def normalize_model(value: str) -> str:
    compact = re.sub(r"[^a-z0-9]", "", value.lower())
    aliases = {
        "kerneldirect": "kernel-direct",
        "kernel": "kernel-direct",
        "tensorapi": "tensor-api",
        "regbase": "regbase",
        "simd": "simd",
        "simt": "simt",
        "simdsimtmixed": "simd-simt-mixed",
    }
    return aliases.get(compact, value.strip().lower())


def normalize_invocation_mode(value: str) -> str:
    """Normalize a host-facing invocation style without inventing new modes."""
    return normalize_model(value)


def normalize_execution_path(value: str) -> str:
    """Normalize the AICore API/execution path used by the current kernel."""
    return normalize_model(value)


def normalize(dimension: str, value: str) -> str:
    if value == "*":
        return value
    if dimension == "cann":
        return normalize_cann(value)
    if dimension == "soc":
        return normalize_soc(value)
    if dimension == "npu_arch":
        return normalize_arch(value)
    if dimension == "programming_model":
        return normalize_model(value)
    raise ValueError(f"unknown scope dimension: {dimension}")


def facts_path() -> Path:
    facts = Path(__file__).with_name("npu-arch-facts.json")
    if facts.is_file():
        return facts
    raise ValueError("NPU architecture facts are unavailable")


def load_architecture_facts() -> dict[str, Any]:
    return json.loads(facts_path().read_text(encoding="utf-8"))


def compact_name(value: str) -> str:
    return re.sub(r"[^a-z0-9]", "", value.lower())


@dataclass(frozen=True)
class SocIdentity:
    """A catalog-resolved SoC identifier without inventing product precision.

    ``products`` in the architecture facts are exact product identifiers.  The
    family ``soc_version`` and ``aliases`` only identify a DAV family, so they
    must never be silently promoted to one product from that family.
    """

    soc: str | None
    kind: str
    family: str | None
    npu_arch: str | None


def _matches_identifier(requested: str, candidate: str) -> bool:
    normalized = compact_name(candidate)
    return requested in {normalized, normalized.removeprefix("ascend")}


def _matches_product_variant(requested: str, candidate: str) -> bool:
    """Map a delimited numeric device suffix to its exact product."""
    normalized = candidate.casefold()
    bases = {normalized, normalized.removeprefix("ascend")}
    return any(
        base
        and not base[-1].isdigit()
        and re.fullmatch(rf"{re.escape(base)}[_-][0-9]+", requested.casefold())
        for base in bases
    )


def resolve_soc_identity(value: str | None) -> SocIdentity:
    """Resolve a SoC token from the facts table, preserving its precision.

    Product entries take priority because ``Ascend910_93`` is also present in a
    historical family-alias list.  Values known only as a family alias retain
    that alias, without pretending to
    know an exact product.  Unknown values remain unknown and infer no arch.
    """

    if value is None or not value.strip():
        return SocIdentity(None, "unknown", None, None)
    requested = compact_name(value)
    families = load_architecture_facts()["families"]
    for family in families:
        for product in family["products"]:
            if _matches_identifier(requested, str(product)):
                return SocIdentity(
                    str(product),
                    "exact-product",
                    str(family["soc_version"]),
                    str(family["npu_arch"]),
                )
    for family in families:
        for product in family["products"]:
            if _matches_product_variant(value.strip(), str(product)):
                return SocIdentity(
                    str(product),
                    "exact-product",
                    str(family["soc_version"]),
                    str(family["npu_arch"]),
                )
    for family in families:
        arch = str(family["npu_arch"])
        family_name = str(family["soc_version"])
        for alias in (family_name, *family["aliases"]):
            if _matches_identifier(requested, str(alias)):
                return SocIdentity(str(alias), "family-alias", family_name, arch)
    return SocIdentity(value.strip(), "unknown", None, None)


def resolve_profile(
    *,
    cann: str | None,
    soc: str | None,
    npu_arch: str | None,
    programming_model: str | None,
    invocation_mode: str | None = None,
    execution_path: str | None = None,
) -> dict[str, Any]:
    """Normalize one profile and reject contradictory explicit identities.

    This is the one resolver used by environment detection and public CLIs.
    An explicit architecture may supplement an unknown SoC, but it may not
    contradict a catalog-resolved product or family alias.
    """

    identity = resolve_soc_identity(soc)
    explicit_arch = normalize_arch(npu_arch) if npu_arch else None
    if explicit_arch and identity.npu_arch and explicit_arch != identity.npu_arch:
        raise ValueError(
            "SoC/arch conflict: "
            f"{identity.soc} requires DAV {identity.npu_arch}, got DAV {explicit_arch}"
        )
    normalized_model = normalize_model(programming_model) if programming_model else None
    normalized_invocation = (
        normalize_invocation_mode(invocation_mode) if invocation_mode else None
    )
    normalized_execution = (
        normalize_execution_path(execution_path) if execution_path else None
    )
    if normalized_model in INVOCATION_MODELS:
        if normalized_invocation and normalized_invocation != normalized_model:
            raise ValueError("programming model/invocation conflict")
        normalized_invocation = normalized_invocation or normalized_model
    elif normalized_model in EXECUTION_PATHS:
        if normalized_execution and normalized_execution != normalized_model:
            raise ValueError("programming model/execution path conflict")
        normalized_execution = normalized_execution or normalized_model
    if normalized_invocation and normalized_invocation not in INVOCATION_MODELS:
        raise ValueError(f"unsupported invocation mode: {normalized_invocation}")
    if normalized_execution and normalized_execution not in EXECUTION_PATHS:
        raise ValueError(f"unsupported execution path: {normalized_execution}")
    return {
        "cann": normalize_cann(cann) if cann else None,
        "soc": identity.soc,
        "soc_identity_kind": identity.kind,
        "soc_family": identity.family,
        "derived_npu_arch": identity.npu_arch,
        "npu_arch": explicit_arch or identity.npu_arch,
        "programming_model": normalized_model,
        "invocation_mode": normalized_invocation,
        "execution_path": normalized_execution,
    }


def resolve_input_profile(
    *,
    cann: str | None,
    soc: str | None,
    npu_arch: str | None,
    programming_model: str | None,
    invocation_mode: str | None = None,
    execution_path: str | None = None,
    environ: dict[str, str] | None = None,
    require_cann: bool = True,
) -> dict[str, Any]:
    """Resolve CLI input without mixing an explicit SoC with host hardware.

    A supplied SoC is an identity boundary, not a partial preference.  Its
    architecture is either supplied alongside it or derived from the facts
    catalog, and its programming model is supplied or intentionally unknown.
    When CANN is omitted in that mode, only CANN is detected; ``detect_profile``
    is deliberately not called because it would inspect unrelated host
    hardware.  Without an explicit SoC, host detection remains the fallback
    and individual explicit dimensions may override it.
    """

    values = dict(os.environ if environ is None else environ)
    if soc is not None:
        effective_cann = cann
        if effective_cann is None:
            effective_cann, _ = detect_cann(values)
        if effective_cann is None and require_cann:
            raise ValueError(
                "CANN version is required; pass --cann or configure ASC_PRECISION_CANN_VERSION"
            )
        return resolve_profile(
            cann=effective_cann,
            soc=soc,
            npu_arch=npu_arch,
            programming_model=programming_model,
            invocation_mode=invocation_mode,
            execution_path=execution_path,
        )

    detected = detect_profile(values)
    effective_cann = cann or detected["cann"]
    if effective_cann is None and require_cann:
        raise ValueError(
            "CANN version is required; pass --cann or configure ASC_PRECISION_CANN_VERSION"
        )
    return resolve_profile(
        cann=effective_cann,
        soc=detected["soc"],
        npu_arch=npu_arch or detected["npu_arch"],
        programming_model=programming_model or detected["programming_model"],
        invocation_mode=invocation_mode or detected.get("invocation_mode"),
        execution_path=execution_path or detected.get("execution_path"),
    )


def soc_from_board_name(board_name: str) -> str | None:
    for family in load_architecture_facts()["families"]:
        mapped = family.get("board_names", {}).get(board_name)
        if isinstance(mapped, str) and mapped:
            return mapped
    return None


def soc_from_device_name(device_name: str) -> str | None:
    """Map npu-smi product tokens such as ``910B1`` to catalog SoC names."""
    identity = resolve_soc_identity(device_name)
    return identity.soc if identity.kind != "unknown" else None


def detect_cann(environ: dict[str, str]) -> tuple[str | None, list[str]]:
    evidence: list[str] = []
    explicit = environ.get("ASC_PRECISION_CANN_VERSION")
    if explicit:
        return normalize_cann(explicit), ["env:ASC_PRECISION_CANN_VERSION"]
    candidates = [
        environ.get("ASCEND_HOME_PATH"),
        environ.get("ASCEND_TOOLKIT_HOME"),
        environ.get("ASC_HOME_PATH"),
    ]
    for raw in candidates:
        if not raw:
            continue
        match = re.search(r"cann[-_/]?(\d+\.\d+\.\d+)", raw, re.IGNORECASE)
        if match:
            return match.group(1), [f"path:{raw}"]
        root = Path(raw)
        for name in ("version.cfg", "version.info", "ascend_toolkit_install.info"):
            path = root / name
            if not path.is_file():
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            version = re.search(r"\d+\.\d+\.\d+", text)
            if version:
                return version.group(0), [f"file:{path}"]
            evidence.append(f"unparsed:{path}")
    return None, evidence


def detect_soc(environ: dict[str, str]) -> tuple[str | None, list[str]]:
    explicit = environ.get("ASC_PRECISION_SOC")
    if explicit:
        return explicit, ["env:ASC_PRECISION_SOC"]
    executable = shutil.which("npu-smi")
    if not executable:
        return None, []
    try:
        listing = subprocess.run(
            [executable, "info", "-l"],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        listing = None
    if listing is not None and listing.returncode == 0:
        npu_ids = re.findall(
            r"^\s*NPU ID\s*:\s*(\d+)\s*$", listing.stdout, re.MULTILINE
        )
        for npu_id in npu_ids:
            try:
                board = subprocess.run(
                    [executable, "info", "-t", "board", "-i", npu_id, "-c", "0"],
                    capture_output=True,
                    text=True,
                    timeout=5,
                    check=False,
                )
            except (OSError, subprocess.TimeoutExpired):
                continue
            if board.returncode:
                continue
            board_match = re.search(
                r"^\s*NPU Name\s*:\s*(\S+)\s*$", board.stdout, re.MULTILINE
            )
            chip_match = re.search(
                r"^\s*Chip Name\s*:\s*(\S+)\s*$", board.stdout, re.MULTILINE
            )
            if board_match or chip_match:
                raw_name = (board_match or chip_match).group(1)
                mapped = soc_from_board_name(raw_name) or soc_from_device_name(raw_name)
                if mapped:
                    mapping_kind = "board" if board_match else "chip"
                    return mapped, [
                        "command:npu-smi info -l",
                        f"command:npu-smi info -t board -i {npu_id} -c 0",
                        f"mapping:{mapping_kind}:{raw_name}->{mapped}",
                    ]
    try:
        result = subprocess.run(
            [executable, "info"], capture_output=True, text=True, timeout=5, check=False
        )
    except (OSError, subprocess.TimeoutExpired):
        return None, []
    text = result.stdout + "\n" + result.stderr
    match = re.search(r"Ascend\s*\d+[A-Za-z0-9_]*", text, re.IGNORECASE)
    if match:
        return re.sub(r"\s+", "", match.group(0)), ["command:npu-smi info"]
    table_match = re.search(
        r"^\|\s*\d+\s+([0-9][A-Za-z0-9_]*)\s+\|", text, re.MULTILINE
    )
    mapped = soc_from_device_name(table_match.group(1)) if table_match else None
    return (mapped, ["command:npu-smi info"]) if mapped else (None, [])


def infer_arch(
    soc: str | None, environ: dict[str, str]
) -> tuple[str | None, list[str]]:
    explicit = environ.get("ASC_PRECISION_NPU_ARCH")
    if explicit:
        return normalize_arch(explicit), ["env:ASC_PRECISION_NPU_ARCH"]
    identity = resolve_soc_identity(soc)
    if not identity.npu_arch:
        return None, []
    return identity.npu_arch, [f"mapping:{identity.soc}->{identity.npu_arch}"]


def detect_profile(environ: dict[str, str] | None = None) -> dict[str, Any]:
    values = dict(os.environ if environ is None else environ)
    cann, cann_evidence = detect_cann(values)
    soc, soc_evidence = detect_soc(values)
    arch, arch_evidence = infer_arch(soc, values)
    profile = resolve_profile(
        cann=cann,
        soc=soc,
        npu_arch=arch,
        programming_model=values.get("ASC_PRECISION_PROGRAMMING_MODEL"),
        invocation_mode=values.get("ASC_PRECISION_INVOCATION_MODE"),
        execution_path=values.get("ASC_PRECISION_EXECUTION_PATH"),
    )
    profile["complete_for_versioned_query"] = profile["cann"] is not None
    profile["evidence"] = cann_evidence + soc_evidence + arch_evidence
    return profile
