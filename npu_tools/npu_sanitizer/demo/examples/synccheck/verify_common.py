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
from pathlib import Path


def verify(case, expected_summary, expected_diagnostics):
    if len(sys.argv) != 2:
        raise ValueError(f"usage: {sys.argv[0]} <npu-check-output>")

    output = Path(sys.argv[1]).read_text(encoding="utf-8")

    summaries = re.findall(r"^(tool=synccheck .*)$", output, re.M)
    if len(summaries) != 1:
        raise ValueError(
            f"{case}: expected one synccheck summary, got {len(summaries)}"
        )
    actual_summary = dict(re.findall(r"([a-z_]+)=([0-9]+)", summaries[0]))
    for key, expected in expected_summary.items():
        actual = actual_summary.get(key)
        if actual != str(expected):
            raise ValueError(f"{case}: expected {key}={expected}, got {actual}")

    for diagnostic in expected_diagnostics:
        if diagnostic not in output:
            raise ValueError(f"{case}: missing diagnostic: {diagnostic}")
    if not expected_diagnostics and "npu_check: DIAGNOSTIC" in output:
        raise ValueError(f"{case}: unexpected diagnostic")

    sessions = re.findall(r"^(status=.*)$", output, re.M)
    if len(sessions) != 1:
        raise ValueError(f"{case}: expected one session result, got {len(sessions)}")
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

    outcomes = re.findall(r"^(\[CLI\] outcome=.*)$", output, re.M)
    if len(outcomes) != 1:
        raise ValueError(f"{case}: expected one CLI outcome, got {len(outcomes)}")
    actual_outcome = dict(re.findall(r"([a-z_]+)=([^ ]+)", outcomes[0]))
    expected_outcome = {
        "outcome": "forwarded",
        "has_errors": "1" if expected_summary["errors"] != 0 else "0",
        "truncated": "0",
        "child_exit": "0",
    }
    for key, expected in expected_outcome.items():
        actual = actual_outcome.get(key)
        if actual != expected:
            raise ValueError(f"{case}: expected CLI {key}={expected}, got {actual}")

    print(f"synccheck verification passed: {case}")
