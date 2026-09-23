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

"""profile_detection 工具单元测试。

用途：验证正常结果、失败处理及输入边界。
使用方法（在 Skill 根目录执行）：
    python3 -m pytest tests/test_profile_detection.py -q
"""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys

import pytest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import platform_profile as profile  # noqa: E402
from platform_profile import (  # noqa: E402
    detect_profile,
    load_architecture_facts,
    resolve_input_profile,
    resolve_profile,
)


@pytest.fixture(autouse=True)
def no_host_hardware(monkeypatch):
    monkeypatch.setattr(profile.shutil, "which", lambda _name: None)


def test_explicit_profile_environment_is_deterministic():
    result = detect_profile(
        {
            "ASC_PRECISION_CANN_VERSION": "CANN 9.0.0",
            "ASC_PRECISION_SOC": "Ascend950PR",
            "ASC_PRECISION_NPU_ARCH": "dav-3510",
            "ASC_PRECISION_PROGRAMMING_MODEL": "regbase",
        }
    )
    assert result["cann"] == "9.0.0"
    assert result["soc"] == "Ascend950PR"
    assert result["npu_arch"] == "3510"
    assert result["programming_model"] == "regbase"
    assert result["complete_for_versioned_query"] is True


def test_soc_mapping_does_not_bind_cann_version():
    result = detect_profile(
        {
            "ASC_PRECISION_CANN_VERSION": "9.0.0",
            "ASC_PRECISION_SOC": "Ascend950PR",
        }
    )
    assert result["cann"] == "9.0.0"
    assert result["npu_arch"] == "3510"


def test_architecture_facts_cover_all_91_canonical_calls():
    facts = load_architecture_facts()
    assert "schema_version" not in facts
    assert "source" not in facts
    assert set(facts["canonical_chips"]) == {
        "ascend910",
        "ascend910b1",
        "ascend310p",
        "ascend610",
        "ascend310b1",
        "ascend950pr_9599",
    }
    dav2201 = next(item for item in facts["families"] if item["npu_arch"] == "2201")
    assert dav2201["board_names"]["9362"] == "Ascend910_93"


def test_all_documented_product_aliases_map_to_the_expected_architecture():
    cases = {
        "Ascend910": "1001",
        "Ascend910B1": "2201",
        "Ascend910B4": "2201",
        "Ascend910B2C": "2201",
        "Ascend910_93": "2201",
        "Ascend310P1": "2002",
        "Ascend310P3": "2002",
        "Ascend610": "2002",
        "Ascend310B1": "3002",
        "Ascend310B4": "3002",
        "Ascend950PR": "3510",
        "Ascend950DT": "3510",
    }
    for soc, expected in cases.items():
        result = detect_profile(
            {"ASC_PRECISION_CANN_VERSION": "9.1.0", "ASC_PRECISION_SOC": soc}
        )
        assert result["npu_arch"] == expected, soc


def test_exact_products_and_family_aliases_keep_distinct_identity_precision():
    cases = {
        "A2": ("A2", "family-alias", "ASCEND910B", "2201"),
        "A3": ("A3", "family-alias", "ASCEND910B", "2201"),
        "A5": ("A5", "family-alias", "ASCEND950", "3510"),
        "Ascend910B1": ("Ascend910B1", "exact-product", "ASCEND910B", "2201"),
        "Ascend910_93": ("Ascend910_93", "exact-product", "ASCEND910B", "2201"),
        "Ascend950PR": ("Ascend950PR", "exact-product", "ASCEND950", "3510"),
        "Ascend950DT": ("Ascend950DT", "exact-product", "ASCEND950", "3510"),
        "Unknown950": ("Unknown950", "unknown", None, None),
    }
    for input_soc, expected in cases.items():
        result = detect_profile(
            {
                "ASC_PRECISION_CANN_VERSION": "9.0.0",
                "ASC_PRECISION_SOC": input_soc,
            }
        )
        assert (
            result["soc"],
            result["soc_identity_kind"],
            result["soc_family"],
            result["derived_npu_arch"],
        ) == expected
        assert result["npu_arch"] == expected[3]


def test_profile_normalizes_variants_and_rejects_soc_arch_conflicts():
    profile = resolve_profile(
        cann="CANN 9.0.0",
        soc="ascend950pr",
        npu_arch="DAV 3510",
        programming_model="SIMD_SIMT_MIXED",
    )
    assert profile["cann"] == "9.0.0"
    assert profile["soc"] == "Ascend950PR"
    assert profile["npu_arch"] == "3510"
    assert profile["programming_model"] == "simd-simt-mixed"

    device_variant = resolve_profile(
        cann="9.0.0",
        soc="950PR_9589",
        npu_arch="dav-3510",
        programming_model="kernel-direct",
    )
    assert device_variant["soc"] == "Ascend950PR"
    assert device_variant["soc_identity_kind"] == "exact-product"

    unknown_numeric_variant = resolve_profile(
        cann="9.0.0",
        soc="Ascend910_94",
        npu_arch=None,
        programming_model="kernel-direct",
    )
    assert unknown_numeric_variant["soc_identity_kind"] == "unknown"
    assert unknown_numeric_variant["npu_arch"] is None

    try:
        resolve_profile(
            cann="9.0.0",
            soc="A2",
            npu_arch="dav-3510",
            programming_model="kernel",
        )
    except ValueError as exc:
        assert "SoC/arch conflict" in str(exc)
    else:
        raise AssertionError("contradictory SoC and arch must fail closed")


def test_profile_keeps_invocation_and_execution_path_orthogonal():
    mixed = resolve_profile(
        cann="9.0.0",
        soc="Ascend950PR",
        npu_arch="3510",
        programming_model="Kernel直调",
        execution_path="RegBase",
    )
    assert mixed["programming_model"] == "kernel-direct"
    assert mixed["invocation_mode"] == "kernel-direct"
    assert mixed["execution_path"] == "regbase"

    explicit = resolve_profile(
        cann="9.0.0",
        soc="Ascend950PR",
        npu_arch="3510",
        programming_model=None,
        invocation_mode="kernel-direct",
        execution_path="simt",
    )
    assert explicit["invocation_mode"] == "kernel-direct"
    assert explicit["execution_path"] == "simt"

    try:
        resolve_profile(
            cann="9.0.0",
            soc="Ascend950PR",
            npu_arch="3510",
            programming_model="regbase",
            execution_path="simt",
        )
    except ValueError as exc:
        assert "execution path conflict" in str(exc)
    else:
        raise AssertionError("conflicting legacy and explicit paths must fail closed")


def test_explicit_soc_override_rederives_instead_of_inheriting_host_arch(monkeypatch):
    monkeypatch.setattr(
        profile,
        "detect_profile",
        lambda _env: (_ for _ in ()).throw(
            AssertionError("explicit SoC must not probe host hardware")
        ),
    )
    resolved = resolve_input_profile(
        cann="9.0.0", soc="A2", npu_arch=None, programming_model="kernel-direct"
    )
    assert resolved["soc"] == "A2"
    assert resolved["soc_identity_kind"] == "family-alias"
    assert resolved["npu_arch"] == "2201"


def test_explicit_soc_isolated_from_host_identity_and_its_missing_model_is_unknown(
    monkeypatch,
):
    hostile = {
        "ASC_PRECISION_CANN_VERSION": "9.0.0",
        "ASC_PRECISION_SOC": "A5",
        "ASC_PRECISION_NPU_ARCH": "3510",
        "ASC_PRECISION_PROGRAMMING_MODEL": "regbase",
    }
    monkeypatch.setattr(
        profile,
        "detect_profile",
        lambda _env: (_ for _ in ()).throw(
            AssertionError("explicit SoC must not call detect_profile")
        ),
    )
    full = resolve_input_profile(
        cann="9.0.0",
        soc="A2",
        npu_arch="2201",
        programming_model="kernel-direct",
        environ=hostile,
    )
    assert (full["soc"], full["npu_arch"], full["programming_model"]) == (
        "A2",
        "2201",
        "kernel-direct",
    )

    soc_only = resolve_input_profile(
        cann=None,
        soc="A2",
        npu_arch=None,
        programming_model=None,
        environ=hostile,
    )
    assert (
        soc_only["cann"],
        soc_only["soc"],
        soc_only["npu_arch"],
        soc_only["programming_model"],
    ) == (
        "9.0.0",
        "A2",
        "2201",
        None,
    )

    try:
        resolve_input_profile(
            cann="9.0.0",
            soc="A2",
            npu_arch="3510",
            programming_model=None,
            environ=hostile,
        )
    except ValueError as exc:
        assert "SoC/arch conflict" in str(exc)
    else:
        raise AssertionError("contradictory explicit profile must fail closed")


def test_board_name_takes_priority_over_ambiguous_generic_chip_name(
    monkeypatch, tmp_path
):
    def fake_run(argv, **_kwargs):
        if argv[1:] == ["info", "-l"]:
            return subprocess.CompletedProcess(
                argv, 0, "NPU ID : 5\nChip Count : 2\n", ""
            )
        if argv[1:] == ["info", "-t", "board", "-i", "5", "-c", "0"]:
            return subprocess.CompletedProcess(
                argv, 0, "NPU Name : 9362\nChip Name : Ascend910\n", ""
            )
        if argv[1:] == ["info"]:
            return subprocess.CompletedProcess(argv, 0, "Ascend910\n", "")
        raise AssertionError(argv)

    monkeypatch.setattr(
        profile.shutil, "which", lambda _name: str(tmp_path / "npu-smi")
    )
    monkeypatch.setattr(profile.subprocess, "run", fake_run)
    result = detect_profile({"ASC_PRECISION_CANN_VERSION": "9.0.0"})
    assert result["soc"] == "Ascend910_93"
    assert result["npu_arch"] == "2201"
    assert "mapping:board:9362->Ascend910_93" in result["evidence"]


def test_chip_name_without_ascend_prefix_maps_to_catalog_product(monkeypatch, tmp_path):
    def fake_run(argv, **_kwargs):
        if argv[1:] == ["info", "-l"]:
            return subprocess.CompletedProcess(argv, 0, "NPU ID : 1\n", "")
        if argv[1:] == ["info", "-t", "board", "-i", "1", "-c", "0"]:
            return subprocess.CompletedProcess(argv, 0, "Chip Name : 910B1\n", "")
        raise AssertionError(argv)

    monkeypatch.setattr(
        profile.shutil, "which", lambda _name: str(tmp_path / "npu-smi")
    )
    monkeypatch.setattr(profile.subprocess, "run", fake_run)
    result = detect_profile({"ASC_PRECISION_CANN_VERSION": "9.0.0"})
    assert result["soc"] == "Ascend910B1"
    assert result["npu_arch"] == "2201"
    assert "mapping:chip:910B1->Ascend910B1" in result["evidence"]
