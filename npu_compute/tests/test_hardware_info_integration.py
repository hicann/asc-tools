# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
import json
import os
import subprocess
from pathlib import Path


BIN_DIR = Path(os.environ["NPU_COMPUTE_TEST_BIN_DIR"])
CALLBACK_STUB = Path(os.environ["NPU_COMPUTE_CALLBACK_STUB"])
HARDWARE_API_STUB = Path(os.environ["NPU_COMPUTE_HARDWARE_API_STUB"])
CLI = BIN_DIR / "npu-compute"
APP = BIN_DIR / "npu_compute_demo_app"


def extract_staging_path(stderr):
    prefix = "npu-compute: staging="
    for line in stderr.splitlines():
        if line.startswith(prefix):
            return Path(line[len(prefix) :])
    raise AssertionError(f"missing staging path in:\n{stderr}")


def test_cli_profapi_callback_collector_and_jsonl_end_to_end():
    environment = os.environ.copy()
    existing_preload = environment.get("LD_PRELOAD", "")
    environment["LD_PRELOAD"] = os.pathsep.join(
        value
        for value in (str(CALLBACK_STUB), str(HARDWARE_API_STUB), existing_preload)
        if value
    )
    existing_library_path = environment.get("LD_LIBRARY_PATH", "")
    environment["LD_LIBRARY_PATH"] = os.pathsep.join(
        value for value in (str(BIN_DIR), existing_library_path) if value
    )
    environment["NPU_COMPUTE_TEST_CALLBACK_EVENTS"] = "1"

    result = subprocess.run(
        [str(CLI), "--section", "PipeUtilization", str(APP)],
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "[demo] completed" in result.stderr
    assert result.stderr.count("[acl_pti_callback_stub] subscribe") == 1
    assert result.stderr.count("[acl_pti_callback_stub] enable=1") == 3
    assert result.stderr.count("[acl_pti_callback_stub] enable=0") == 3
    assert result.stderr.count("[acl_pti_callback_stub] event") == 4
    assert result.stderr.count("site=0 retval=0 dispatched=1") == 2
    assert result.stderr.count("site=1 retval=0 dispatched=1") == 2
    assert result.stderr.count("[hardware_api_stub] aclrtGetDeviceCount") == 1
    assert "[aclpti] initialize dependencies" not in result.stderr

    staging = extract_staging_path(result.stderr)
    hardware_info = staging / "HardwareInfo.jsonl"
    assert staging.is_dir()
    assert hardware_info.is_file()
    assert not hardware_info.is_symlink()
    assert list(staging.glob("HardwareInfo*.jsonl")) == [hardware_info]

    content = hardware_info.read_text(encoding="utf-8")
    lines = content.splitlines()
    assert len(lines) == 5
    host = json.loads(lines[0])
    assert list(host) == [
        "category",
        "cpu physical count",
        "cpu logical count",
        "memory total size(MB)",
        "disk total size(GB)",
    ]
    assert host["category"] == "Host Info"
    for field in (
        "cpu physical count",
        "cpu logical count",
        "memory total size(MB)",
        "disk total size(GB)",
    ):
        assert host[field] >= 0
    expected = (
        "\n".join(
            [
                lines[0],
                '{"category":"Device Info","npu count":1,'
                '"chip info":"Ascend950PR_9599 V100","arch info":"3510"}',
                '{"category":"CPU Information","control cpu count":4,'
                '"ai cpu count":8,"ai cpu frequency(MHZ)":1500}',
                '{"category":"AI Core Information","ai core count":32,'
                '"ai cube count":16,"ai vector count":16,'
                '"ai cube frequency(MHZ)":1800,"ai vector frequency(MHZ)":1600}',
                '{"category":"Memory Information","hbm total(MB)":65536,'
                '"hbm used(MB)":16384,"hbm frequency(MHZ)":3200}',
            ]
        )
        + "\n"
    )
    assert content == expected


def test_cli_real_aclpti_callback_chain():
    environment = os.environ.copy()
    existing_preload = environment.get("LD_PRELOAD", "")
    environment["LD_PRELOAD"] = os.pathsep.join(
        value for value in (str(HARDWARE_API_STUB), existing_preload) if value
    )
    existing_library_path = environment.get("LD_LIBRARY_PATH", "")
    environment["LD_LIBRARY_PATH"] = os.pathsep.join(
        value for value in (str(BIN_DIR), existing_library_path) if value
    )
    environment["NPU_COMPUTE_DEBUG"] = "1"
    environment.pop("NPU_COMPUTE_TEST_CALLBACK_EVENTS", None)

    result = subprocess.run(
        [str(CLI), "--section", "PipeUtilization", str(APP)],
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert "[aclpti] subscribe result=0" in result.stderr
    for cbid in (9, 4, 13):
        assert f"[libnpu-compute] enabled ACL PTI callback cbid={cbid}" in result.stderr
        assert (
            f"[libnpu-compute] disabled ACL PTI callback cbid={cbid}" in result.stderr
        )
    for cbid in (4, 13):
        assert f"runtime callback domain=1 cbid={cbid} site=0" in result.stderr
        assert (
            f"runtime callback domain=1 cbid={cbid} site=1 retval=0 accepted=1"
            in result.stderr
        )
    assert "disable ACL PTI callback failed" not in result.stderr

    staging = extract_staging_path(result.stderr)
    hardware_info = staging / "HardwareInfo.jsonl"
    assert hardware_info.is_file()
    assert len(hardware_info.read_text(encoding="utf-8").splitlines()) == 5
    assert list(staging.glob("HardwareInfo*.jsonl")) == [hardware_info]
