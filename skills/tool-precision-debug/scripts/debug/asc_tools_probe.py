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

"""用途：按安装根和目标架构检查官方工具入口、CPU 库导入及 npu check 组件；支持新旧布局，不编译或运行目标算子。
使用方法（在 Skill 根目录执行）：
    python3 scripts/debug/asc_tools_probe.py --help

参数和示例见 scripts/usage/asc-tools-probe.md。
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import signal
import shutil
import subprocess
import sys
import tempfile
from typing import Any


sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from platform_profile import load_architecture_facts  # noqa: E402


SCHEMA_VERSION = "1.1.0"
CAPABILITY_IDS = (
    "cpu-debug",
    "npu-check",
    "msobjdump",
    "show-kernel-debug-data",
)
ENVIRONMENT_ROOTS = ("ASCEND_HOME_PATH", "ASCEND_TOOLKIT_HOME")
CLI_LAYOUTS = {
    "msobjdump": Path("tools/msobjdump/msobjdump"),
    "show-kernel-debug-data": Path(
        "tools/show_kernel_debug_data/show_kernel_debug_data"
    ),
}
VERSION_FILE = Path("share/info/asc-tools/version.info")
VERSION_RE = re.compile(r"(?:CANN\s*)?(\d+\.\d+\.\d+)", re.IGNORECASE)
# Official installation layouts; resolve every component within the same layout.
CPU_DEBUG_LAYOUTS = (
    (
        "cpudebug",
        "tools/cpudebug/lib64",
        "tools/cpudebug/include",
        "tools/cpudebug/cmake",
    ),
    (
        "tikicpulib",
        "tools/tikicpulib/lib",
        "tools/tikicpulib/lib/include",
        "tools/tikicpulib/lib/cmake",
    ),
)
CMAKE_TARGET_PROBE = """\
cmake_minimum_required(VERSION 3.16)
project(asc_tools_probe LANGUAGES NONE)
find_package(${PROBE_PACKAGE} REQUIRED CONFIG)
if(NOT TARGET "tikicpulib::${PROBE_TARGET}")
  message(FATAL_ERROR "CPU Debug target tikicpulib::${PROBE_TARGET} is unavailable")
endif()
"""


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Probe the installed Ascend C Tools components without running a target "
            "operator workload"
        )
    )
    parser.add_argument(
        "--cann-root",
        type=Path,
        help="Probe only this CANN installation root; disables environment/PATH discovery",
    )
    parser.add_argument(
        "--expected-cann",
        help="Require an exact CANN release match when evaluating --require",
    )
    parser.add_argument(
        "--npu-arch",
        help=(
            "Target NPU architecture, for example 2201 or dav-2201; required for "
            "cpu-debug and npu-check to become available"
        ),
    )
    parser.add_argument(
        "--require",
        action="append",
        default=[],
        choices=CAPABILITY_IDS,
        help="Return 1 unless this capability is invocable/complete in one compatible root",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=3.0,
        help="Seconds allowed for each help or CMake target probe (default: 3)",
    )
    parser.add_argument(
        "--json", action="store_true", help="Accepted for CLI consistency"
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Atomically write the JSON probe report to a new file as well as stdout",
    )
    return parser


def write_report(path_value: Path, rendered: str) -> None:
    path = path_value.expanduser().resolve()
    if path.exists():
        raise ValueError(f"--output already exists: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{path.name}.", suffix=".tmp", dir=path.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            stream.write(rendered)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def normalize_version(value: str | None) -> str | None:
    if value is None:
        return None
    match = VERSION_RE.fullmatch(value.strip())
    if not match:
        raise ValueError(
            "--expected-cann must be an exact semantic release such as 9.0.0"
        )
    return match.group(1)


def normalize_architecture(value: str | None) -> tuple[str | None, str | None]:
    if value is None:
        return None, None
    normalized = value.strip().lower()
    if normalized.startswith("dav-"):
        normalized = normalized[4:]
    facts = load_architecture_facts()
    for family in facts["families"]:
        if str(family["npu_arch"]) == normalized:
            calls = family.get("canonical_calls", [])
            if not calls:
                raise ValueError(
                    f"architecture {normalized} has no canonical CPU Debug target"
                )
            return normalized, str(calls[0])
    raise ValueError(f"unsupported --npu-arch: {value!r}")


def normalized_path(value: Path | str) -> Path:
    return Path(value).expanduser().resolve(strict=False)


def path_state(path: Path, *, executable: bool = False) -> str:
    try:
        path.stat()
    except PermissionError:
        return "inaccessible"
    except OSError:
        return "missing"
    if not path.is_file():
        return "missing"
    if executable and not os.access(path, os.X_OK):
        return "inaccessible"
    if not os.access(path, os.R_OK):
        return "inaccessible"
    return "present"


def add_candidate(
    candidates: dict[str, set[str]], root: Path | str, source: str
) -> None:
    normalized = str(normalized_path(root))
    candidates.setdefault(normalized, set()).add(source)


def root_from_command(command: str, executable: Path) -> Path | None:
    layout = CLI_LAYOUTS[command]
    resolved = normalized_path(executable)
    parts = layout.parts
    if len(resolved.parts) <= len(parts) or resolved.parts[-len(parts) :] != parts:
        return None
    return Path(*resolved.parts[: -len(parts)])


def discover_roots(
    explicit: Path | None, environment: dict[str, str]
) -> tuple[list[dict[str, Any]], dict[str, str | None]]:
    candidates: dict[str, set[str]] = {}
    path_commands: dict[str, str | None] = {}
    if explicit is not None:
        add_candidate(candidates, explicit, "explicit")
        return _candidate_records(candidates), {name: None for name in CLI_LAYOUTS}

    for name in ENVIRONMENT_ROOTS:
        value = environment.get(name)
        if value:
            add_candidate(candidates, value, f"environment:{name}")
    search_path = environment.get("PATH", "")
    for name in CLI_LAYOUTS:
        command = "show_kernel_debug_data" if name == "show-kernel-debug-data" else name
        executable = shutil.which(command, path=search_path)
        path_commands[name] = str(normalized_path(executable)) if executable else None
        if executable:
            root = root_from_command(name, Path(executable))
            if root is not None:
                add_candidate(candidates, root, f"path:{name}")
    return _candidate_records(candidates), path_commands


def _candidate_records(candidates: dict[str, set[str]]) -> list[dict[str, Any]]:
    return [
        {"root": root, "sources": sorted(sources)}
        for root, sources in sorted(candidates.items())
    ]


def read_version(root: Path, expected: str | None) -> dict[str, Any]:
    path = root / VERSION_FILE
    state = path_state(path)
    value: str | None = None
    error: str | None = None
    if state == "present":
        try:
            fields: dict[str, str] = {}
            for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
                key, separator, raw = line.partition("=")
                if separator:
                    fields[key.strip().lower()] = raw.strip().strip("\"'")
            candidate = fields.get("version")
            if candidate:
                match = VERSION_RE.fullmatch(candidate)
                value = match.group(1) if match else None
                if value is None:
                    error = f"unrecognized Version field: {candidate!r}"
            else:
                error = "Version field is missing"
        except OSError as exc:
            state = "inaccessible"
            error = str(exc)
    elif state == "inaccessible":
        error = "version file is not readable"

    if expected is None:
        match_status = "not-requested"
    elif value is None:
        match_status = "unknown"
    elif value == expected:
        match_status = "match"
    else:
        match_status = "mismatch"
    return {
        "value": value,
        "source": str(path),
        "source_status": state,
        "match": match_status,
        "error": error,
    }


def component(name: str, path: Path, *, executable: bool = False) -> dict[str, str]:
    return {
        "name": name,
        "path": str(path),
        "status": path_state(path, executable=executable),
    }


def command_component(name: str, executable: str | None) -> dict[str, str]:
    if executable is None:
        return {"name": name, "path": name, "status": "missing"}
    return component(name, Path(executable), executable=True)


def status_from_components(components: list[dict[str, str]]) -> str:
    states = [item["status"] for item in components]
    if states and all(state == "present" for state in states):
        return "available"
    if "inaccessible" in states:
        return "inaccessible"
    if "present" in states:
        return "partial"
    return "unavailable"


def degradation(status: str, components: list[dict[str, str]]) -> str | None:
    if status == "available":
        return None
    failures = [
        f"{item['name']}={item['status']}"
        for item in components
        if item["status"] != "present"
    ]
    return "Required component set is incomplete: " + ", ".join(failures)


def product_libraries(library_root: Path) -> tuple[list[str], str]:
    libraries: list[str] = []
    state = "missing"
    try:
        inaccessible = False
        for directory in library_root.iterdir():
            candidate = directory / "libcpudebug.so"
            candidate_state = path_state(candidate)
            if candidate_state == "present":
                libraries.append(str(candidate))
            elif candidate_state == "inaccessible":
                inaccessible = True
        libraries.sort()
        if libraries:
            state = "present"
        elif inaccessible:
            state = "inaccessible"
    except PermissionError:
        state = "inaccessible"
    except OSError:
        state = "missing"
    return libraries, state


def installed_cmake_target(config: Path, canonical: str) -> str | None:
    try:
        content = config.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return None
    product_lists = re.findall(
        r"set\(PRODUCT_TYPE_LIST_[^)]*\)", content, flags=re.DOTALL
    )
    tokens = re.findall(r"[A-Za-z0-9_-]+", "\n".join(product_lists))
    exact = next((token for token in tokens if token == canonical), None)
    if exact is not None:
        return exact
    return next((token for token in tokens if token.lower() == canonical.lower()), None)


def stop_process(process: subprocess.Popen[Any]) -> None:
    try:
        if os.name == "posix":
            os.killpg(process.pid, signal.SIGKILL)
        else:
            process.kill()
    except ProcessLookupError:
        pass
    try:
        process.wait(timeout=1)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def run_quiet(
    command: list[str], *, cwd: Path, timeout: float, timeout_error: str
) -> dict[str, Any]:
    process: subprocess.Popen[Any] | None = None
    try:
        process = subprocess.Popen(
            command,
            cwd=cwd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=(os.name == "posix"),
        )
        process.wait(timeout=timeout)
        return {
            "attempted": True,
            "returncode": process.returncode,
            "timed_out": False,
            "error": None,
        }
    except subprocess.TimeoutExpired:
        assert process is not None
        stop_process(process)
        return {
            "attempted": True,
            "returncode": None,
            "timed_out": True,
            "error": timeout_error,
        }
    except OSError as exc:
        return {
            "attempted": True,
            "returncode": None,
            "timed_out": False,
            "error": str(exc),
        }


def cmake_target_probe(
    cmake: str,
    cmake_config: Path,
    package: str,
    npu_arch: str,
    target: str,
    timeout: float,
) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="asc-tools-probe-") as temporary:
        source = Path(temporary) / "source"
        build = Path(temporary) / "build"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(CMAKE_TARGET_PROBE, encoding="utf-8")
        result = run_quiet(
            [
                cmake,
                "-S",
                str(source),
                "-B",
                str(build),
                f"-D{package}_DIR={cmake_config.parent}",
                f"-DPROBE_PACKAGE={package}",
                f"-DPROBE_TARGET={target}",
            ],
            cwd=source,
            timeout=timeout,
            timeout_error="CMake CPU Debug target probe timed out",
        )
    return {
        **result,
        "npu_arch": npu_arch,
        "cmake_target": f"tikicpulib::{target}",
    }


def probe_cpu_debug_layout(
    root: Path,
    npu_arch: str | None,
    canonical_target: str | None,
    timeout: float,
    environment: dict[str, str],
    layout: tuple[str, str, str, str],
) -> dict[str, Any]:
    package, library_relative, include_relative, cmake_relative = layout
    library_root = root / library_relative
    cmake_config = root / cmake_relative / f"{package}-config.cmake"
    cmake = shutil.which("cmake", path=environment.get("PATH", ""))
    fixed = [
        component("header", root / include_relative / "tikicpulib.h"),
        component("cmake-config", cmake_config),
        component("stub-registration", library_root / "libcpudebug_stubreg.so"),
        component("debug-print", library_root / "libcpudebug_cceprint.so"),
        component("runtime-instrumentation", library_root / "libcpudebug_npuchk.so"),
        command_component("cmake", cmake),
    ]
    discovered_libraries, product_state = product_libraries(library_root)
    product_component = {
        "name": "product-library",
        "path": str(library_root / "*/libcpudebug.so"),
        "status": product_state,
    }
    components = [*fixed, product_component]
    status = status_from_components(components)
    target_probe: dict[str, Any] = {
        "attempted": False,
        "returncode": None,
        "timed_out": False,
        "error": None,
        "npu_arch": npu_arch,
        "cmake_target": None,
    }
    failure = degradation(status, components)
    if status == "available" and npu_arch is None:
        status = "partial"
        failure = "--npu-arch is required to verify a target CPU Debug CMake target"
    elif status == "available":
        assert canonical_target is not None
        target = installed_cmake_target(cmake_config, canonical_target)
        if target is None:
            status = "unusable"
            failure = (
                f"CMake package does not declare the canonical target {canonical_target!r} "
                f"for DAV {npu_arch}"
            )
            target_probe["error"] = failure
        else:
            assert cmake is not None
            target_probe = cmake_target_probe(
                cmake, cmake_config, package, npu_arch, target, timeout
            )
            if target_probe["timed_out"] or target_probe["returncode"] != 0:
                status = "unusable"
                failure = target_probe["error"] or (
                    f"CMake target probe exited {target_probe['returncode']}"
                )
    return {
        "id": "cpu-debug",
        "status": status,
        "layout": package,
        "probe_method": "cmake-target",
        "entrypoint": None,
        "components": components,
        "dependencies": [],
        "product_libraries": discovered_libraries,
        "help_probe": None,
        "target_probe": target_probe,
        "degradation": failure,
    }


def probe_cpu_debug(
    root: Path,
    npu_arch: str | None,
    canonical_target: str | None,
    timeout: float,
    environment: dict[str, str],
) -> dict[str, Any]:
    results = []
    for layout in CPU_DEBUG_LAYOUTS:
        result = probe_cpu_debug_layout(
            root, npu_arch, canonical_target, timeout, environment, layout
        )
        if result["status"] == "available":
            return result
        results.append(result)
    # Keep the most informative incomplete layout, without combining components.
    return max(
        results,
        key=lambda result: sum(
            item["status"] == "present" for item in result["components"]
        ),
    )


def probe_npu_check(root: Path, cpu_debug: dict[str, Any]) -> dict[str, Any]:
    instrumentation = next(
        item
        for item in cpu_debug["components"]
        if item["name"] == "runtime-instrumentation"
    )
    components = [
        component(
            "report-script",
            root / "tools/ascendc_tools/ascendc_npuchk_report.py",
        ),
        dict(instrumentation),
    ]
    status = status_from_components(components)
    failure = degradation(status, components)
    if status == "available" and cpu_debug["status"] != "available":
        status = "partial"
        failure = (
            "npu check components are installed, but the target CPU Debug capability "
            f"is {cpu_debug['status']}"
        )
    entrypoint = components[0]["path"] if components[0]["status"] == "present" else None
    return {
        "id": "npu-check",
        "status": status,
        "probe_method": "dependent-component-set",
        "entrypoint": entrypoint,
        "components": components,
        "dependencies": ["cpu-debug"],
        "product_libraries": [],
        "help_probe": None,
        "target_probe": dict(cpu_debug["target_probe"]),
        "degradation": failure,
    }


def run_help(executable: Path, timeout: float) -> dict[str, Any]:
    return run_quiet(
        [str(executable), "-h"],
        cwd=executable.parent,
        timeout=timeout,
        timeout_error="help probe timed out",
    )


def probe_cli(root: Path, capability_id: str, timeout: float) -> dict[str, Any]:
    executable = root / CLI_LAYOUTS[capability_id]
    components = [component("executable", executable, executable=True)]
    status = status_from_components(components)
    help_probe: dict[str, Any] | None = None
    if status == "available":
        help_probe = run_help(executable, timeout)
        if help_probe["timed_out"] or help_probe["returncode"] != 0:
            status = "unusable"
    failure = degradation(status, components)
    if status == "unusable":
        assert help_probe is not None
        failure = help_probe["error"] or f"help probe exited {help_probe['returncode']}"
    return {
        "id": capability_id,
        "status": status,
        "probe_method": "read-only-help",
        "entrypoint": str(executable) if components[0]["status"] == "present" else None,
        "components": components,
        "dependencies": [],
        "product_libraries": [],
        "help_probe": help_probe,
        "target_probe": None,
        "degradation": failure,
    }


def installation_status(version_match: str, capabilities: list[dict[str, Any]]) -> str:
    states = [item["status"] for item in capabilities]
    if version_match == "mismatch":
        return "incompatible"
    if all(state == "available" for state in states):
        return "available"
    if any(state == "available" for state in states):
        return "partial"
    if any(state in {"partial", "inaccessible", "unusable"} for state in states):
        return "partial"
    return "unavailable"


def probe_installation(
    candidate: dict[str, Any],
    expected: str | None,
    npu_arch: str | None,
    canonical_target: str | None,
    timeout: float,
    environment: dict[str, str],
) -> dict[str, Any]:
    root = Path(candidate["root"])
    version = read_version(root, expected)
    cpu_debug = probe_cpu_debug(root, npu_arch, canonical_target, timeout, environment)
    capabilities = [
        cpu_debug,
        probe_npu_check(root, cpu_debug),
        probe_cli(root, "msobjdump", timeout),
        probe_cli(root, "show-kernel-debug-data", timeout),
    ]
    return {
        "root": str(root),
        "sources": candidate["sources"],
        "version": version,
        "status": installation_status(version["match"], capabilities),
        "capabilities": capabilities,
    }


def evaluate_requirements(
    requested: list[str], installations: list[dict[str, Any]], expected: str | None
) -> dict[str, Any]:
    unique = sorted(set(requested))
    missing: list[str] = []
    for capability_id in unique:
        satisfied = False
        for installation in installations:
            version_match = installation["version"]["match"]
            compatible = version_match == "match" if expected else True
            capability = next(
                item
                for item in installation["capabilities"]
                if item["id"] == capability_id
            )
            if compatible and capability["status"] == "available":
                satisfied = True
                break
        if not satisfied:
            missing.append(capability_id)
    return {"requested": unique, "satisfied": not missing, "missing": missing}


def overall_status(installations: list[dict[str, Any]]) -> str:
    statuses = [item["status"] for item in installations]
    if not statuses or all(item == "unavailable" for item in statuses):
        return "unavailable"
    if all(item == "incompatible" for item in statuses):
        return "incompatible"
    if any(item == "available" for item in statuses):
        return "available"
    return "partial"


def validate_report_semantics(report: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    installations = report["installations"]
    candidates = report["discovery"]["candidates"]
    candidate_identity = [(item["root"], item["sources"]) for item in candidates]
    installation_identity = [(item["root"], item["sources"]) for item in installations]
    if candidate_identity != installation_identity:
        errors.append("discovery candidates do not match installations")
    for installation in installations:
        capabilities = installation["capabilities"]
        capability_ids = [item["id"] for item in capabilities]
        if len(capability_ids) != len(CAPABILITY_IDS) or set(capability_ids) != set(
            CAPABILITY_IDS
        ):
            errors.append(f"capability set is invalid for {installation['root']}")
            continue
        expected_status = installation_status(
            installation["version"]["match"], capabilities
        )
        if installation["status"] != expected_status:
            errors.append(
                f"installation status is inconsistent for {installation['root']}"
            )
        by_id = {item["id"]: item for item in capabilities}
        cpu_debug = by_id["cpu-debug"]
        npu_check = by_id["npu-check"]
        if npu_check["status"] == "available" and cpu_debug["status"] != "available":
            errors.append(
                f"npu check lacks CPU Debug readiness for {installation['root']}"
            )
        if npu_check["dependencies"] != ["cpu-debug"]:
            errors.append(f"npu check dependency is invalid for {installation['root']}")
        if cpu_debug["status"] == "available":
            target_probe = cpu_debug["target_probe"]
            if (
                not target_probe["attempted"]
                or target_probe["timed_out"]
                or target_probe["returncode"] != 0
            ):
                errors.append(
                    f"CPU Debug target proof is invalid for {installation['root']}"
                )
    if report["status"] != overall_status(installations):
        errors.append("overall status is inconsistent")
    expected_requirements = evaluate_requirements(
        report["requirements"]["requested"],
        installations,
        report["expected_cann"],
    )
    if report["requirements"] != expected_requirements:
        errors.append("requirements are inconsistent")
    return errors


def execute(
    args: argparse.Namespace, environment: dict[str, str] | None = None
) -> dict[str, Any]:
    if not 0 < args.timeout <= 30:
        raise ValueError("--timeout must be greater than 0 and no more than 30 seconds")
    expected = normalize_version(args.expected_cann)
    npu_arch, canonical_target = normalize_architecture(args.npu_arch)
    active_environment = dict(os.environ if environment is None else environment)
    candidates, path_commands = discover_roots(args.cann_root, active_environment)
    installations = [
        probe_installation(
            candidate,
            expected,
            npu_arch,
            canonical_target,
            args.timeout,
            active_environment,
        )
        for candidate in candidates
    ]
    requirements = evaluate_requirements(args.require, installations, expected)
    report = {
        "schema_version": SCHEMA_VERSION,
        "mode": "probe",
        "expected_cann": expected,
        "npu_arch": npu_arch,
        "status": overall_status(installations),
        "discovery": {
            "explicit_root": args.cann_root is not None,
            "environment_roots": {
                name: active_environment.get(name) for name in ENVIRONMENT_ROOTS
            },
            "path_executables": path_commands,
            "candidates": candidates,
        },
        "installations": installations,
        "requirements": requirements,
        "policy": {
            "installation_read_only": True,
            "temporary_cmake_configure": True,
            "executes_target_workload": False,
            "unavailable_means_not_run": True,
            "cross_root_component_merge": False,
        },
    }
    semantic_errors = validate_report_semantics(report)
    if semantic_errors:
        raise ValueError(
            "internal probe report invariant failed: " + "; ".join(semantic_errors)
        )
    return report


def main() -> int:
    args = build_parser().parse_args()
    try:
        report = execute(args)
    except (OSError, ValueError) as exc:
        print(
            json.dumps({"status": "INVALID", "error": str(exc)}, ensure_ascii=False),
            file=sys.stderr,
        )
        return 2
    rendered = json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
    try:
        if args.output is not None:
            write_report(args.output, rendered)
    except (OSError, ValueError) as exc:
        print(
            json.dumps({"status": "INVALID", "error": str(exc)}, ensure_ascii=False),
            file=sys.stderr,
        )
        return 2
    print(rendered, end="")
    return 0 if report["requirements"]["satisfied"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
