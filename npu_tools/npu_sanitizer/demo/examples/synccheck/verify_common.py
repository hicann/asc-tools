#!/usr/bin/python3
# coding=utf-8

# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

import re
import sys
import time
from collections import Counter
from pathlib import Path


TERMINAL_PATTERN = re.compile(r"^\[CLI\] outcome=.*$", re.M)


def read_complete_output(path, timeout_seconds=2.0):
    deadline = time.monotonic() + timeout_seconds
    while True:
        output = path.read_text(encoding="utf-8")
        if TERMINAL_PATTERN.search(output) or time.monotonic() >= deadline:
            return output
        time.sleep(0.02)


def verify(case, expected_summary, expected_diagnostics):
    if len(sys.argv) != 3:
        raise ValueError(f"usage: {sys.argv[0]} <npu-check-output> <npu-check-status>")

    output = read_complete_output(Path(sys.argv[1]))
    actual_status = int(sys.argv[2])
    if actual_status != 0:
        raise ValueError(f"{case}: expected npu_check status 0, got {actual_status}")

    summaries = re.findall(r"^(tool=synccheck .*)$", output, re.M)
    sessions = re.findall(r"^(status=.*)$", output, re.M)
    terminals = re.findall(r"^(\[CLI\] outcome=.*)$", output, re.M)

    if len(summaries) != 1:
        raise ValueError(
            f"{case}: expected one synccheck summary, got {len(summaries)}"
        )
    if len(sessions) != 1:
        raise ValueError(f"{case}: expected one session result, got {len(sessions)}")
    if len(terminals) != 1:
        raise ValueError(f"{case}: expected one terminal result, got {len(terminals)}")

    actual_summary = dict(re.findall(r"([a-z_]+)=([0-9]+)", summaries[0]))
    for key, expected in expected_summary.items():
        actual = actual_summary.get(key)
        if actual != str(expected):
            raise ValueError(f"{case}: expected {key}={expected}, got {actual}")

    actual_diagnostics = Counter(
        re.findall(
            r"^========= ERROR:\[SYNCCHECK\] "
            r"(Synchronization pairing mismatch: .*)$",
            output,
            re.M,
        )
    )
    expected_diagnostic_counts = Counter(expected_diagnostics)
    if actual_diagnostics != expected_diagnostic_counts:
        raise ValueError(
            f"{case}: expected diagnostics {dict(expected_diagnostic_counts)}, "
            f"got {dict(actual_diagnostics)}"
        )

    actual_session = dict(re.findall(r"([a-z_]+)=([^ ]+)", sessions[0]))
    expected_session = {
        "status": "complete",
        "aclsan_unsubscribe": "0",
        "dropped_messages": "0",
        "analysis_complete": "true",
        "report_truncated": "false",
    }
    for key, expected in expected_session.items():
        actual = actual_session.get(key)
        if actual != expected:
            raise ValueError(f"{case}: expected session {key}={expected}, got {actual}")

    actual_terminal = dict(re.findall(r"([a-z_]+)=([^ ]+)", terminals[0]))
    expected_terminal = {
        "outcome": "forwarded",
        "has_errors": "1" if expected_summary["errors"] != 0 else "0",
        "truncated": "0",
        "child_exit": "0",
        "exit": str(actual_status),
    }
    for key, expected in expected_terminal.items():
        actual = actual_terminal.get(key)
        if actual != expected:
            raise ValueError(f"{case}: expected CLI {key}={expected}, got {actual}")

    print(f"synccheck verification passed: {case}")
