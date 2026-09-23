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

"""用途：执行构建、复现、实验或回归，保存真实退出码、完整日志和执行元数据；长运行在 stderr 报告可关闭的进度 heartbeat。
使用方法（在 Skill 根目录执行）：
    python3 scripts/debug/run_with_evidence.py --help

参数和示例见 scripts/usage/run-with-evidence.md。
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import sys
import time
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from probe_binding import ProbeBindingError, bind_probe_report


SCHEMA_VERSION = "1.0.0"
SHELL_NAMES = {"bash", "dash", "ksh", "sh", "zsh"}
PROCESS_GROUP_POLL_SECONDS = 0.02
DEFAULT_HEARTBEAT_INTERVAL_SECONDS = 30.0


def now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def executable_identity(
    command: list[str], cwd: Path, environment: dict[str, str]
) -> dict[str, Any] | None:
    if not command:
        return None
    token = command[0]
    if "/" in token:
        candidate = Path(token)
        candidate = cwd / candidate if not candidate.is_absolute() else candidate
    else:
        # exec resolves relative PATH entries after changing to the child's cwd.
        search_path = os.pathsep.join(
            str(cwd / entry) for entry in os.get_exec_path(environment)
        )
        located = shutil.which(token, path=search_path)
        if not located:
            return None
        candidate = Path(located)
    resolved = candidate.resolve()
    if not resolved.is_file():
        return None
    return {
        "path": str(resolved),
        # Keep the launch spelling: resolving a Python venv symlink can change
        # interpreter environment discovery even though it names the same file.
        "launch_path": str(candidate),
        "size_bytes": resolved.stat().st_size,
        "sha256_before_execution": sha256_file(resolved),
    }


def validation_target_identity(
    target_text: str | None,
    command: list[str],
    cwd: Path,
    executable: dict[str, Any] | None,
) -> dict[str, Any] | None:
    """Freeze a safe target and prove adoption only from the launched argv."""
    if target_text is None:
        return None
    target = Path(target_text)
    result: dict[str, Any] = {
        "path": target.as_posix(),
        "sha256": None,
        "size_bytes": None,
        "adoption_method": "unavailable",
        "adopted": False,
    }
    candidate = cwd / target
    try:
        metadata = candidate.lstat()
    except OSError:
        result["reason"] = "missing"
        return result
    if candidate.is_symlink():
        result["reason"] = "symlink-not-allowed"
        return result
    if not candidate.is_file():
        result["reason"] = "not-a-regular-file"
        return result
    resolved = candidate.resolve()
    try:
        resolved.relative_to(cwd)
    except ValueError:
        result["reason"] = "escapes-cwd"
        return result
    result.update({"sha256": sha256_file(candidate), "size_bytes": metadata.st_size})
    if executable is not None and executable["path"] == str(resolved):
        result.update({"adopted": True, "adoption_method": "resolved-executable"})
        return result
    if _is_interpreter_script(command, executable, cwd, resolved):
        result.update({"adopted": True, "adoption_method": "interpreter-script"})
        return result
    result.update({"adoption_method": "not-in-executed-argv", "reason": "not-adopted"})
    return result


def _is_interpreter_script(
    command: list[str],
    executable: dict[str, Any] | None,
    cwd: Path,
    target: Path,
) -> bool:
    """Prove only the ordinary ``interpreter [options] script`` form."""
    if executable is None or len(command) < 2:
        return False
    interpreter = Path(str(executable["path"])).name.lower()
    if not interpreter.startswith(("python", "pypy")):
        return False
    index = 1
    options_with_value = {"-W", "-X"}
    while index < len(command):
        token = command[index]
        if token in {"-c", "-m"}:
            return False
        if token in options_with_value:
            index += 2
            continue
        if token.startswith("-"):
            index += 1
            continue
        script = Path(token)
        if script.is_absolute() or ".." in script.parts:
            return False
        candidate = cwd / script
        return (
            not candidate.is_symlink()
            and candidate.is_file()
            and candidate.resolve() == target
        )
    return False


def write_json_atomic(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    try:
        temporary.write_text(
            json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def shell_script(command: list[str]) -> str | None:
    if not command or Path(command[0]).name not in SHELL_NAMES:
        return None
    for index, token in enumerate(command[:-1]):
        if token in {"-c", "-lc"}:
            return command[index + 1]
    return None


def has_naked_shell_pipeline(command: list[str]) -> bool:
    script = shell_script(command)
    if not script:
        return False
    try:
        lexer = shlex.shlex(script, posix=True, punctuation_chars="|&;")
        lexer.whitespace_split = True
        lexer.commenters = ""
        tokens = list(lexer)
    except ValueError:
        return False
    has_pipeline = any(token in {"|", "|&"} for token in tokens)
    return has_pipeline and "pipefail" not in tokens


def render_tail(path: Path, line_count: int) -> None:
    if line_count <= 0 or not path.is_file():
        return
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for line in lines[-line_count:]:
        print(line)


def emit_heartbeat(log_path: Path, started_monotonic: float) -> None:
    """Report wrapper progress without adding wrapper output to the evidence log."""
    try:
        log_size: int | str = log_path.stat().st_size
    except OSError:
        log_size = "unavailable"
    elapsed = time.monotonic() - started_monotonic
    print(
        "[run_with_evidence] heartbeat "
        f"elapsed_seconds={elapsed:.1f} log_size_bytes={log_size}",
        file=sys.stderr,
        flush=True,
    )


def wait_for_process(
    process: subprocess.Popen[str],
    log_path: Path,
    *,
    timeout: float | None,
    heartbeat_interval: float,
) -> int:
    """Wait for completion while reporting progress and preserving one deadline."""
    if heartbeat_interval == 0:
        return process.wait(timeout=timeout)

    started_monotonic = time.monotonic()
    deadline = started_monotonic + timeout if timeout is not None else None
    next_heartbeat = started_monotonic + heartbeat_interval
    while True:
        completed = process.poll()
        if completed is not None:
            return completed

        now = time.monotonic()
        if deadline is not None and now >= deadline:
            completed = process.poll()
            if completed is not None:
                return completed
            raise subprocess.TimeoutExpired(process.args, timeout)
        if now >= next_heartbeat:
            emit_heartbeat(log_path, started_monotonic)
            next_heartbeat = now + heartbeat_interval
            continue

        next_event = next_heartbeat
        if deadline is not None:
            next_event = min(next_event, deadline)
        try:
            return process.wait(timeout=next_event - now)
        except subprocess.TimeoutExpired:
            # Re-evaluate both boundaries after an interval wake-up. This also
            # makes the no-timeout path progress-visible instead of blocking
            # forever in one wait call.
            continue


def process_group_exists(process_group_id: int) -> bool:
    try:
        os.killpg(process_group_id, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def wait_for_process_group_exit(
    process: subprocess.Popen[str],
    process_group_id: int,
    timeout: float,
) -> bool:
    deadline = time.monotonic() + timeout
    while True:
        process.poll()
        if not process_group_exists(process_group_id):
            return True
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        time.sleep(min(PROCESS_GROUP_POLL_SECONDS, remaining))


def terminate_timed_out_process(
    process: subprocess.Popen[str], grace_seconds: float
) -> dict[str, Any]:
    details: dict[str, Any] = {
        "strategy": "posix_process_group" if os.name == "posix" else "direct_process",
        "grace_seconds": grace_seconds,
        "sigterm_sent": False,
        "sigkill_sent": False,
        "cleanup_errors": [],
    }

    if os.name == "posix":
        process_group_id = process.pid
        details["process_group_id"] = process_group_id
        try:
            os.killpg(process_group_id, signal.SIGTERM)
            details["sigterm_sent"] = True
        except ProcessLookupError:
            pass
        except OSError as error:
            details["cleanup_errors"].append(
                f"SIGTERM process group failed: {type(error).__name__}: {error}"
            )

        if not wait_for_process_group_exit(process, process_group_id, grace_seconds):
            try:
                os.killpg(process_group_id, signal.SIGKILL)
                details["sigkill_sent"] = True
            except ProcessLookupError:
                pass
            except OSError as error:
                details["cleanup_errors"].append(
                    f"SIGKILL process group failed: {type(error).__name__}: {error}"
                )
            wait_for_process_group_exit(process, process_group_id, 1.0)

        try:
            process.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            try:
                process.kill()
                process.wait()
                details["direct_kill_fallback"] = True
            except OSError as error:
                details["cleanup_errors"].append(
                    f"direct kill fallback failed: {type(error).__name__}: {error}"
                )
        details["group_present_after_cleanup"] = process_group_exists(process_group_id)
    else:
        try:
            process.terminate()
            details["sigterm_sent"] = True
        except OSError as error:
            details["cleanup_errors"].append(
                f"terminate failed: {type(error).__name__}: {error}"
            )
        try:
            process.wait(timeout=grace_seconds)
        except subprocess.TimeoutExpired:
            try:
                process.kill()
                details["sigkill_sent"] = True
                process.wait()
            except OSError as error:
                details["cleanup_errors"].append(
                    f"kill failed: {type(error).__name__}: {error}"
                )

    details["direct_process_returncode_after_cleanup"] = process.returncode
    details["cleanup_complete"] = (
        not details["cleanup_errors"]
        and not details.get("group_present_after_cleanup", False)
        and process.returncode is not None
    )
    return details


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "执行构建、测试或复现命令，保存合并日志、真实子进程退出码和元数据；"
            "需要预览时使用 --tail-lines，不要在外部把命令管道给 tail/grep。"
        )
    )
    parser.add_argument("--log", required=True, help="stdout/stderr 合并日志路径")
    parser.add_argument("--metadata", help="执行元数据 JSON；默认 <log>.meta.json")
    parser.add_argument("--cwd", help="子进程工作目录；默认当前目录")
    parser.add_argument("--timeout", type=float, help="超时秒数；超时返回 124")
    parser.add_argument(
        "--heartbeat-interval",
        type=float,
        default=DEFAULT_HEARTBEAT_INTERVAL_SECONDS,
        help=(
            "运行期间向 stderr 输出进度的间隔秒数（含 elapsed 和日志大小）；"
            f"默认 {DEFAULT_HEARTBEAT_INTERVAL_SECONDS:g}，传 0 关闭"
        ),
    )
    parser.add_argument(
        "--terminate-grace",
        type=float,
        default=1.0,
        help="超时后发送 SIGTERM 的宽限秒数，之后强制清理进程组；默认 1.0",
    )
    parser.add_argument(
        "--tail-lines",
        type=int,
        default=0,
        help="进程结束后由本工具打印日志末尾 N 行，不改变子进程退出码",
    )
    parser.add_argument("--label", help="写入元数据的证据标签")
    parser.add_argument(
        "--validation-target",
        help="待验证的相对目标文件；仅当它实际是执行文件或 argv 参数时记录采用身份",
    )
    parser.add_argument(
        "--env",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="向子进程显式注入环境变量；元数据只保存名称和值的 SHA-256",
    )
    parser.add_argument("--probe-report", help="asc_tools_probe.py 生成的 JSON")
    parser.add_argument(
        "--require-capability",
        action="append",
        default=[],
        choices=("cpu-debug", "npu-check", "msobjdump", "show-kernel-debug-data"),
        help="执行前要求 probe 报告显式满足的能力；可重复",
    )
    parser.add_argument("--expected-cann", help="能力绑定使用的 CANN 版本")
    parser.add_argument("--npu-arch", help="能力绑定使用的 NPU Arch，例如 2201")
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="允许覆盖同名日志和元数据；基线证据默认禁止覆盖",
    )
    parser.add_argument(
        "command",
        nargs=argparse.REMAINDER,
        help="在 -- 后给出命令及参数，例如 -- python test.py",
    )
    return parser


def validate_args(args: argparse.Namespace) -> list[str]:
    errors: list[str] = []
    if args.command and args.command[0] == "--":
        args.command = args.command[1:]
    if not args.command:
        errors.append("必须在 -- 后提供待执行命令")
    if args.timeout is not None and (
        not math.isfinite(args.timeout) or args.timeout <= 0
    ):
        errors.append("--timeout 必须是大于 0 的有限数值")
    if not math.isfinite(args.heartbeat_interval) or args.heartbeat_interval < 0:
        errors.append("--heartbeat-interval 必须是大于等于 0 的有限数值")
    if not math.isfinite(args.terminate_grace) or args.terminate_grace < 0:
        errors.append("--terminate-grace 必须是大于等于 0 的有限数值")
    if args.tail_lines < 0:
        errors.append("--tail-lines 不得小于 0")
    if args.cwd and not Path(args.cwd).is_dir():
        errors.append(f"--cwd 目录不存在：{args.cwd}")
    if args.validation_target:
        candidate = Path(args.validation_target)
        if candidate.is_absolute() or ".." in candidate.parts:
            errors.append("--validation-target 必须是 cwd 内的相对路径")
    if args.require_capability and not all(
        (args.probe_report, args.expected_cann, args.npu_arch)
    ):
        errors.append(
            "--require-capability 需要同时提供 --probe-report、--expected-cann 和 --npu-arch"
        )
    if args.probe_report and not args.require_capability:
        errors.append("--probe-report 必须与 --require-capability 一起使用")
    environment_names: set[str] = set()
    for item in args.env:
        name, separator, _ = item.partition("=")
        if (
            not separator
            or not name
            or not name.replace("_", "a").isalnum()
            or name[0].isdigit()
        ):
            errors.append(f"--env 必须使用合法 NAME=VALUE：{item!r}")
        elif name in environment_names:
            errors.append(f"--env 变量重复：{name}")
        environment_names.add(name)

    log_path = Path(args.log).resolve()
    metadata_path = (
        Path(args.metadata).resolve()
        if args.metadata
        else Path(f"{log_path}.meta.json")
    )
    if log_path == metadata_path:
        errors.append("--log 与 --metadata 不能是同一路径")
    if not args.overwrite:
        for label, path in (("日志", log_path), ("元数据", metadata_path)):
            if path.exists():
                errors.append(f"{label}已存在，使用新路径或显式传 --overwrite：{path}")
    if has_naked_shell_pipeline(args.command):
        errors.append(
            "检测到 shell 管道但未启用 pipefail；移除管道，或在 shell 脚本开头设置 "
            "`set -o pipefail`。日志预览请改用 --tail-lines"
        )
    return errors


def normalize_returncode(returncode: int) -> int:
    return 128 + abs(returncode) if returncode < 0 else returncode


def main() -> int:
    args = build_parser().parse_args()
    errors = validate_args(args)
    if errors:
        print(
            json.dumps(
                {"error": "invalid arguments", "details": errors, "exit_code": 2},
                ensure_ascii=False,
                indent=2,
            ),
            file=sys.stderr,
        )
        return 2

    try:
        probe_binding = (
            bind_probe_report(
                args.probe_report,
                expected_cann=args.expected_cann,
                npu_arch=args.npu_arch,
                required_capabilities=args.require_capability,
            )
            if args.require_capability
            else None
        )
    except ProbeBindingError as exc:
        print(
            json.dumps(
                {
                    "error": "invalid probe binding",
                    "details": [str(exc)],
                    "exit_code": 2,
                },
                ensure_ascii=False,
                indent=2,
            ),
            file=sys.stderr,
        )
        return 2

    environment_overrides = dict(item.split("=", 1) for item in args.env)
    child_environment = os.environ.copy()
    child_environment.update(environment_overrides)

    log_path = Path(args.log).resolve()
    metadata_path = (
        Path(args.metadata).resolve()
        if args.metadata
        else Path(f"{log_path}.meta.json")
    )
    cwd = Path(args.cwd).resolve() if args.cwd else Path.cwd().resolve()
    log_path.parent.mkdir(parents=True, exist_ok=True)

    started_at = now_iso()
    started_at_ns = time.time_ns()
    start_monotonic = time.monotonic()
    child_returncode: int | None = None
    wrapper_returncode = 0
    outcome = "completed"
    launch_error: str | None = None
    timeout_cleanup: dict[str, Any] | None = None
    identity = executable_identity(args.command, cwd, child_environment)
    target_identity = validation_target_identity(
        args.validation_target, args.command, cwd, identity
    )

    try:
        with log_path.open("w", encoding="utf-8") as log_stream:
            popen_options: dict[str, Any] = {}
            if os.name == "posix":
                popen_options["start_new_session"] = True
            if identity is not None:
                popen_options["executable"] = identity["launch_path"]
            process = subprocess.Popen(
                args.command,
                cwd=str(cwd),
                env=child_environment,
                stdout=log_stream,
                stderr=subprocess.STDOUT,
                text=True,
                **popen_options,
            )
            try:
                child_returncode = wait_for_process(
                    process,
                    log_path,
                    timeout=args.timeout,
                    heartbeat_interval=args.heartbeat_interval,
                )
                wrapper_returncode = normalize_returncode(child_returncode)
            except subprocess.TimeoutExpired as error:
                outcome = "timeout"
                wrapper_returncode = 124
                launch_error = str(error)
                timeout_cleanup = terminate_timed_out_process(
                    process, args.terminate_grace
                )
    except FileNotFoundError as error:
        outcome = "launch_failed"
        wrapper_returncode = 127
        launch_error = str(error)
        log_path.write_text(f"{type(error).__name__}: {error}\n", encoding="utf-8")
    except OSError as error:
        outcome = "launch_failed"
        wrapper_returncode = 126
        launch_error = str(error)
        log_path.write_text(f"{type(error).__name__}: {error}\n", encoding="utf-8")

    if outcome == "launch_failed" and target_identity is not None:
        target_identity.update(
            adopted=False, adoption_method="unavailable", reason="launch-failed"
        )

    payload: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "label": args.label,
        "command_argv": args.command,
        "cwd": str(cwd),
        "started_at": started_at,
        "finished_at": now_iso(),
        "started_at_ns": started_at_ns,
        "finished_at_ns": time.time_ns(),
        "duration_seconds": round(time.monotonic() - start_monotonic, 6),
        "outcome": outcome,
        "child_returncode": child_returncode,
        "wrapper_returncode": wrapper_returncode,
        "timed_out": outcome == "timeout",
        "log": {
            "path": str(log_path),
            "sha256": sha256_file(log_path),
            "size_bytes": log_path.stat().st_size,
        },
    }
    if identity is not None:
        payload["executable"] = identity
    if target_identity is not None:
        payload["validation_target"] = target_identity
    if environment_overrides:
        payload["environment_overrides"] = [
            {
                "name": name,
                "value_sha256": hashlib.sha256(value.encode("utf-8")).hexdigest(),
            }
            for name, value in sorted(environment_overrides.items())
        ]
    if probe_binding is not None:
        payload["probe_binding"] = probe_binding
    if launch_error:
        payload["error"] = launch_error
    if timeout_cleanup is not None:
        payload["timeout_cleanup"] = timeout_cleanup
    write_json_atomic(metadata_path, payload)

    render_tail(log_path, args.tail_lines)
    print(
        f"[run_with_evidence] rc={wrapper_returncode} log={log_path} "
        f"metadata={metadata_path}",
        file=sys.stderr,
    )
    return wrapper_returncode


if __name__ == "__main__":
    raise SystemExit(main())
