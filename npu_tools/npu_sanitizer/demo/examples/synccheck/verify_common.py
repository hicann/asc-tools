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
from pathlib import Path


TERMINAL_PATTERN = re.compile(
    r"^(?:\[CLI\] outcome=.*|"
    r"npu_check: child_exit=[^ ]+ handshake=[^ ]+ session_end=[^ ]+)$",
    re.M,
)


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

    current_summaries = re.findall(r"^(tool=synccheck .*)$", output, re.M)
    legacy_summaries = re.findall(
        r"^npu_check: SUMMARY (tool=synccheck .*)$", output, re.M
    )
    current_sessions = re.findall(r"^(status=.*)$", output, re.M)
    legacy_sessions = re.findall(r"^npu_check: SESSION_END (status=.*)$", output, re.M)
    current_terminals = re.findall(r"^(\[CLI\] outcome=.*)$", output, re.M)
    legacy_terminals = re.findall(
        r"^(npu_check: child_exit=[^ ]+ handshake=[^ ]+ session_end=[^ ]+)$",
        output,
        re.M,
    )

    summary_count = len(current_summaries) + len(legacy_summaries)
    if summary_count != 1:
        raise ValueError(f"{case}: expected one synccheck summary, got {summary_count}")
    session_count = len(current_sessions) + len(legacy_sessions)
    if session_count != 1:
        raise ValueError(f"{case}: expected one session result, got {session_count}")
    terminal_count = len(current_terminals) + len(legacy_terminals)
    if terminal_count != 1:
        raise ValueError(f"{case}: expected one terminal result, got {terminal_count}")

    legacy_parts = (
        bool(legacy_summaries),
        bool(legacy_sessions),
        bool(legacy_terminals),
    )
    if len(set(legacy_parts)) != 1:
        raise ValueError(f"{case}: mixed current and legacy output formats")
    legacy = legacy_parts[0]

    summary = legacy_summaries[0] if legacy else current_summaries[0]
    actual_summary = dict(re.findall(r"([a-z_]+)=([0-9]+)", summary))
    for key, expected in expected_summary.items():
        actual = actual_summary.get(key)
        if actual != str(expected):
            raise ValueError(f"{case}: expected {key}={expected}, got {actual}")

    for diagnostic in expected_diagnostics:
        if diagnostic not in output:
            raise ValueError(f"{case}: missing diagnostic: {diagnostic}")
    if not expected_diagnostics and "npu_check: DIAGNOSTIC" in output:
        raise ValueError(f"{case}: unexpected diagnostic")

    session = legacy_sessions[0] if legacy else current_sessions[0]
    actual_session = dict(re.findall(r"([a-z_]+)=([^ ]+)", session))
    expected_session = {
        "status": "complete",
        "aclsan_unsubscribe": "0",
        "dropped_messages": "0",
        "analysis_complete": "true",
    }
    if not legacy:
        expected_session["report_truncated"] = "false"
    for key, expected in expected_session.items():
        actual = actual_session.get(key)
        if actual != expected:
            raise ValueError(f"{case}: expected session {key}={expected}, got {actual}")

    if legacy:
        actual_terminal = dict(re.findall(r"([a-z_]+)=([^ ]+)", legacy_terminals[0]))
        expected_terminal = {
            "child_exit": "0",
            "handshake": "ready",
            "session_end": "complete",
        }
        terminal_name = "legacy terminal"
    else:
        actual_terminal = dict(re.findall(r"([a-z_]+)=([^ ]+)", current_terminals[0]))
        expected_terminal = {
            "outcome": "forwarded",
            "has_errors": "1" if expected_summary["errors"] != 0 else "0",
            "truncated": "0",
            "child_exit": "0",
            "exit": str(actual_status),
        }
        terminal_name = "CLI"
    for key, expected in expected_terminal.items():
        actual = actual_terminal.get(key)
        if actual != expected:
            raise ValueError(
                f"{case}: expected {terminal_name} {key}={expected}, got {actual}"
            )

    print(f"synccheck verification passed: {case}")
