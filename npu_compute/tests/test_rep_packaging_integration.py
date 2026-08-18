# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
import os
import re
import struct
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


CLI = Path(os.environ["NPU_COMPUTE_TEST_CLI"])
FIXTURE_APP = Path(os.environ["NPU_COMPUTE_REP_FIXTURE_APP"])

MAGIC = b"npu-rep\0"
VERSION = 0x00010000
ORIGIN = 1
HEAD = struct.Struct("<8sIHHIIIQ")
FILE_INFO = struct.Struct("<8s128sHHIQQ")
TYPE_NPU_REP = 1
TYPE_JSONL = 3
TYPE_CSV = 4

HARDWARE_INFO = (
    b'{"category":"Host Info","cpu physical count":1}\n'
    b'{"category":"Device Info","npu count":1}\n'
    b'{"category":"CPU Information","control cpu count":1}\n'
    b'{"category":"AI Core Information","ai core count":1}\n'
    b'{"category":"Memory Information","hbm total(MB)":1}\n'
)
PIPE_CSV = b"block_id,pipe_utilization\n0,75\n"
MEMORY_CSV = b"block_id,read_bytes\n0,128\n"
L2_CACHE_CSV = b"block_id,hit_rate\n0,99\n"


@dataclass
class RepEntry:
    name: str
    file_type: int
    payload: bytes
    offset: int
    child: Optional["DecodedRep"]


@dataclass
class DecodedRep:
    entries: List[RepEntry]


def decode_rep(data: bytes) -> DecodedRep:
    assert len(data) >= HEAD.size
    magic, version, origin, head_size, count, info_size, reserved, length = (
        HEAD.unpack_from(data)
    )
    assert magic == MAGIC
    assert version == VERSION
    assert origin == ORIGIN
    assert head_size == HEAD.size == 36
    assert info_size == FILE_INFO.size == 160
    assert reserved == 0
    assert length == len(data)

    expected_offset = head_size + count * info_size
    assert expected_offset <= len(data)
    entries = []
    for index in range(count):
        info_offset = head_size + index * info_size
        (
            info_magic,
            raw_name,
            file_type,
            reserved16,
            reserved32,
            file_length,
            file_offset,
        ) = FILE_INFO.unpack_from(data, info_offset)
        assert info_magic == MAGIC
        assert reserved16 == 0
        assert reserved32 == 0
        assert file_offset == expected_offset
        assert file_offset + file_length <= len(data)
        nul = raw_name.find(b"\0")
        assert nul > 0
        name = raw_name[:nul].decode("utf-8")
        payload = data[file_offset : file_offset + file_length]
        child = decode_rep(payload) if file_type == TYPE_NPU_REP else None
        entries.append(RepEntry(name, file_type, payload, file_offset, child))
        expected_offset += file_length
    assert expected_offset == len(data)
    return DecodedRep(entries)


def extract_path(stderr: str, key: str) -> Path:
    prefix = f"npu-compute: {key}="
    values = [
        Path(line[len(prefix) :])
        for line in stderr.splitlines()
        if line.startswith(prefix)
    ]
    assert len(values) == 1, stderr
    return values[0]


def run_fixture(tmp_path: Path, exit_code: Optional[int] = None):
    work_directory = tmp_path / "work"
    temporary_root = tmp_path / "tmp"
    work_directory.mkdir()
    temporary_root.mkdir()
    environment = os.environ.copy()
    environment["TMPDIR"] = str(temporary_root)
    environment["NPU_COMPUTE_DEBUG"] = "1"
    command = [
        str(CLI),
        "--section",
        "PipeUtilization",
        str(FIXTURE_APP),
    ]
    if exit_code is not None:
        command.extend(("--exit-code", str(exit_code)))
    result = subprocess.run(
        command,
        cwd=work_directory,
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )
    return result, work_directory


def test_cli_recursively_packages_fixture_files(tmp_path):
    result, work_directory = run_fixture(tmp_path)

    assert result.returncode == 0, result.stderr
    assert "[aclpti]" not in result.stderr
    assert "[prof_api_stub]" not in result.stderr
    staging = extract_path(result.stderr, "staging")
    report = extract_path(result.stderr, "report")
    assert staging.is_absolute() and staging.is_dir()
    assert report.is_absolute() and report.is_file()
    assert report.parent == work_directory.resolve()
    assert re.fullmatch(r"report_[0-9]+_[0-9a-f]{8}\.npu-rep", report.name)
    assert list(work_directory.glob("*.npu-rep")) == [report]

    top = decode_rep(report.read_bytes())
    assert [entry.name for entry in top.entries] == [
        "HardwareInfo.jsonl",
        "PipeUtilization.csv",
        "device_0.npu.rep",
    ]
    assert [entry.file_type for entry in top.entries] == [
        TYPE_JSONL,
        TYPE_CSV,
        TYPE_NPU_REP,
    ]
    assert top.entries[0].offset == HEAD.size + 3 * FILE_INFO.size
    assert top.entries[0].payload == HARDWARE_INFO
    assert top.entries[1].payload == PIPE_CSV

    device = top.entries[2].child
    assert device is not None
    assert [entry.name for entry in device.entries] == [
        "Memory.csv",
        "details.npu.rep",
    ]
    assert device.entries[0].offset == HEAD.size + 2 * FILE_INFO.size
    assert device.entries[0].payload == MEMORY_CSV

    details = device.entries[1].child
    assert details is not None
    assert [entry.name for entry in details.entries] == ["L2Cache.csv"]
    assert details.entries[0].offset == HEAD.size + FILE_INFO.size
    assert details.entries[0].payload == L2_CACHE_CSV


def test_cli_import_recursively_restores_report_files(tmp_path):
    result, work_directory = run_fixture(tmp_path)

    assert result.returncode == 0, result.stderr
    report = extract_path(result.stderr, "report")
    output = tmp_path / "unpacked"
    import_tmp = tmp_path / "import-tmp"
    import_tmp.mkdir()
    environment = os.environ.copy()
    environment["TMPDIR"] = str(import_tmp)
    environment["ACL_API_INJECTION"] = "/npu-compute-import-must-not-load.so"
    imported = subprocess.run(
        [str(CLI), "--import", str(report), "--export", str(output)],
        cwd=work_directory,
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )

    assert imported.returncode == 0, imported.stderr
    assert extract_path(imported.stderr, "unpacked") == output
    assert "npu-compute: staging=" not in imported.stderr
    assert "npu-compute: report=" not in imported.stderr
    assert "[prof_api_stub]" not in imported.stderr
    assert "[aclpti]" not in imported.stderr
    assert list(import_tmp.iterdir()) == []
    files = sorted(
        path.relative_to(output).as_posix()
        for path in output.rglob("*")
        if path.is_file()
    )
    assert files == [
        "HardwareInfo.jsonl",
        "PipeUtilization.csv",
        "device_0/Memory.csv",
        "device_0/details/L2Cache.csv",
    ]
    assert (output / "HardwareInfo.jsonl").read_bytes() == HARDWARE_INFO
    assert (output / "PipeUtilization.csv").read_bytes() == PIPE_CSV
    assert (output / "device_0" / "Memory.csv").read_bytes() == MEMORY_CSV
    assert (
        output / "device_0" / "details" / "L2Cache.csv"
    ).read_bytes() == L2_CACHE_CSV
    assert not (output / ".hardware_info.lock").exists()


def test_failed_app_does_not_publish_report(tmp_path):
    result, work_directory = run_fixture(tmp_path, exit_code=17)

    assert result.returncode == 17
    staging = extract_path(result.stderr, "staging")
    assert staging.is_dir()
    assert "npu-compute: report=" not in result.stderr
    assert list(work_directory.glob("*.npu-rep")) == []
    assert "APP exited with status 17" in result.stderr
