# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
"""Exercise the real instrumenter with controlled compiler/linker executables."""

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


TOOL = r"""
import os
import pathlib
import shutil
import struct
import sys

root = pathlib.Path(os.environ["PROBE_TEST_ROOT"])
tool = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
with (root / "calls").open("a") as log:
    log.write(tool + "\n")
if tool == "bisheng":
    assert "--npu-arch=dav-3510" in args, args
    source = pathlib.Path(args[args.index("-c") + 1])
    assert source.name == "probe.cpp"
    assert b"__npu_compute_before_kernel_end" in source.read_bytes()
    with (root / "probe_sources").open("a") as log:
        log.write(str(source.parent) + "\n")
    pathlib.Path(args[args.index("-o") + 1]).write_bytes(b"\x7fELF" + bytes(4))
elif tool == "ld.lld":
    probe = pathlib.Path(args[args.index("-execute-probe") + 1])
    assert probe.read_bytes().startswith(b"\x7fELF")
    assert (probe.parent / "probe.cpp").exists()
    with (root / "caches").open("a") as log:
        log.write(str(probe.parent) + "\n")
    source = pathlib.Path(args[args.index("-execute-probe") + 2])
    with (root / "workdirs").open("a") as log:
        log.write(str(source.parent) + "\n")
    marker = root / "first-attempt"
    if not marker.exists():
        marker.touch()
        sys.exit(1)
    shutil.copyfile(source, args[args.index("-o") + 1])
else:
    config = pathlib.Path(next(a.split("=", 1)[1] for a in args if a.startswith("--dbi-config=")))
    name = b"__npu_compute_before_kernel_end"
    binding = struct.pack("<HHH", 397, 0, 0)
    names = struct.pack("<I", len(name) + 4) + name + bytes(4)
    header = struct.pack("<IHHHH", 12 + len(binding) + len(names), 0, len(binding), 0, 0)
    assert config.read_bytes() == header + binding + names
    source = args[args.index("--instru-memprobe") + 1]
    output = next(a.split("=", 1)[1] for a in args if a.startswith("-o="))
    shutil.copyfile(source, output)
"""


def main():
    with tempfile.TemporaryDirectory(prefix="npu-probe-test-") as directory:
        root = pathlib.Path(directory)
        executable = root / "instrumenter"
        shutil.copy2(sys.argv[1], executable)
        tools = root / "tools/bisheng_compiler/bin"
        tools.mkdir(parents=True)
        for name in ("bisheng", "ld.lld", "bisheng-tune"):
            script = tools / name
            script.write_text("#!" + sys.executable + "\n" + TOOL)
            script.chmod(0o700)
        env = dict(
            os.environ,
            ASCEND_HOME_PATH=str(root),
            PROBE_TEST_ROOT=str(root),
            PATH=str(tools),
        )
        # Force the mock Bisheng to be used instead of an inherited override.
        env.pop("NPU_COMPUTE_BISHENG", None)
        subprocess.run([str(executable)], env=env, check=True, timeout=60)
        calls = (root / "calls").read_text().splitlines()
        # The probe source is embedded in the library and compiled once by Bisheng on first use.
        assert calls.count("bisheng") == 1, calls
        assert calls.count("ld.lld") == 6, calls
        assert calls.count("bisheng-tune") == 5, calls
        caches = (root / "caches").read_text().splitlines()
        workdirs = (root / "workdirs").read_text().splitlines()
        probe_sources = (root / "probe_sources").read_text().splitlines()
        assert len(set(caches)) == 1
        assert len(set(workdirs)) == 6
        # The probe is compiled in the shared cache directory, not in a per-kernel work directory.
        assert len(set(probe_sources)) == 1
        assert set(probe_sources) == set(caches)
        assert all(
            not pathlib.Path(path).exists()
            for path in caches + workdirs + probe_sources
        )
        assert not (root / "kernel_end").exists()


if __name__ == "__main__":
    main()
