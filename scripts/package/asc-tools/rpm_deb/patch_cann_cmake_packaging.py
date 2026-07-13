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

import argparse
from pathlib import Path


GENERATOR_RELATIVE_PATH = Path("scripts") / "package" / "gen_postinst_prerm.py"
EUID_ROOT_CHECK = 'if [ "$EUID" -eq 0 ]; then'
POSIX_ROOT_CHECK = 'if [ "$(id -u)" -eq 0 ]; then'
TARGET_CLEANUP_REPLACEMENTS = (
    ('rm -rf \\"$INSTALL_PATH\\"/\\"{target}\\"', 'rmdir \\"$INSTALL_PATH\\"/\\"{target}\\" 2>/dev/null || true'),
    ('rm -rf "$INSTALL_PATH"/"{target}"', 'rmdir "$INSTALL_PATH"/"{target}" 2>/dev/null || true'),
)


def patch_content(content):
    patched = content.replace(EUID_ROOT_CHECK, POSIX_ROOT_CHECK)
    for old, new in TARGET_CLEANUP_REPLACEMENTS:
        patched = patched.replace(old, new)

    if "$EUID" in patched:
        raise RuntimeError("failed to remove EUID root checks from gen_postinst_prerm.py")
    for old, _ in TARGET_CLEANUP_REPLACEMENTS:
        if old in patched:
            raise RuntimeError("failed to replace recursive target directory cleanup in gen_postinst_prerm.py")
    return patched


def patch_file(path):
    path = Path(path)
    content = path.read_text()
    patched = patch_content(content)
    if patched != content:
        path.write_text(patched)
    return patched != content


def main():
    parser = argparse.ArgumentParser(description="Patch cann-cmake package generator for asc-tools rpm/deb packaging.")
    parser.add_argument("cann_cmake_dir", help="Path to fetched cann-cmake source directory")
    args = parser.parse_args()

    generator_path = Path(args.cann_cmake_dir) / GENERATOR_RELATIVE_PATH
    if not generator_path.is_file():
        raise FileNotFoundError(generator_path)

    changed = patch_file(generator_path)
    state = "patched" if changed else "already patched"
    print(f"[asc-tools] {state}: {generator_path}")


if __name__ == "__main__":
    main()
