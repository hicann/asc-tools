#! /usr/bin/env python3
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
import os
import unittest
from pathlib import Path


REPO_ROOT = Path(os.path.realpath(__file__)).parents[4]
INSTALL_SCRIPT = REPO_ROOT / "scripts/package/asc-tools/scripts/asc-tools_custom_install.sh"
UNINSTALL_SCRIPT = REPO_ROOT / "scripts/package/asc-tools/scripts/asc-tools_custom_uninstall.sh"
CREATE_SOFTLINK_SCRIPT = REPO_ROOT / "scripts/package/asc-tools/scripts/asc-tools_custom_create_softlink.sh"
REMOVE_SOFTLINK_SCRIPT = REPO_ROOT / "scripts/package/asc-tools/scripts/asc-tools_custom_remove_softlink.sh"
SETUP_SCRIPT = REPO_ROOT / "utils/optype_collector/setup.py"
USER_DOC = REPO_ROOT / "docs/05_optype_collector.md"


class TestOpTypeCollectorPackaging(unittest.TestCase):
    def test_wheel_declares_console_entry_for_bin_symlink_source(self):
        setup_py = SETUP_SCRIPT.read_text(encoding="utf-8")

        self.assertIn("entry_points", setup_py)
        self.assertIn("console_scripts", setup_py)
        self.assertIn("optype_collector=optype_collector.optype_collector_main:main", setup_py)

    def test_install_script_creates_cann_bin_symlink_like_msopgen_msopst(self):
        content = INSTALL_SCRIPT.read_text(encoding="utf-8")
        self.assertIn(
            'createSoftLink "${install_path}/python/site-packages/bin" '
            '"${install_path}/${PLT_ARCH}-linux/bin/" "msopgen"',
            content,
        )
        self.assertIn(
            'createSoftLink "${install_path}/python/site-packages/bin" '
            '"${install_path}/${PLT_ARCH}-linux/bin/" "msopst"',
            content,
        )
        self.assertIn(
            'createSoftLink "${install_path}/python/site-packages/bin" '
            '"${install_path}/${PLT_ARCH}-linux/bin/" "optype_collector"',
            content,
        )
        self.assertNotIn(
            'createSoftLink "${install_path}/tools/optype_collector" "${install_path}/bin" "optype_collector"',
            content,
        )

    def test_optype_collector_install_is_controlled_by_pylocal_like_msobjdump(self):
        content = INSTALL_SCRIPT.read_text(encoding="utf-8")

        self.assertIn('installWhlPackage "${install_path}/tools/msobjdump-0.1.0-py3-none-any.whl"', content)
        self.assertIn('installWhlPackage "${install_path}/tools/optype_collector-0.1.0-py3-none-any.whl"', content)
        self.assertIn('installPyWhlLocal "msobjdump-0.1.0-py3-none-any.whl"', content)
        self.assertIn('installPyWhlLocal "optype_collector-0.1.0-py3-none-any.whl"', content)
        self.assertNotIn('installMsprofWhlPackage "${install_path}/tools/optype_collector', content)

    def test_create_softlink_script_links_optype_collector_to_latest_bin_like_msopgen_msopst(self):
        content = CREATE_SOFTLINK_SCRIPT.read_text(encoding="utf-8")

        self.assertIn('createCanndevSoft "$install_path" "$version_dir" "$latest_dir"', content)
        self.assertNotIn('createOpTypeCollectorBinSoftLink', content)
        self.assertIn('createSoftLink "$_src_dir" "$_dst_dir" "msopgen"', content)
        self.assertIn('createSoftLink "$_src_dir" "$_dst_dir" "msopst"', content)
        self.assertIn('createSoftLink "$_src_dir" "$_dst_dir" "optype_collector"', content)

    def test_remove_softlink_script_removes_optype_collector_from_latest_like_msopgen_msopst(self):
        content = REMOVE_SOFTLINK_SCRIPT.read_text(encoding="utf-8")

        self.assertIn('removeCanndevSoftLink "$install_path" "$latest_dir"', content)
        self.assertIn('removeSoftLink "$_dst_dir" "msopgen"', content)
        self.assertIn('removeSoftLink "$_dst_dir" "msopst"', content)
        self.assertIn('removeSoftLink "$_dst_dir" "optype_collector"', content)
        self.assertIn('removeSoftLink "$_dst_dir" "optype_collector-0.1.0.dist-info"', content)
        self.assertIn('rm -f "$install_path/$latest_dir/tools/optype_collector"', content)

    def test_uninstall_script_removes_optype_collector_bin_symlink_like_msopgen_msopst(self):
        content = UNINSTALL_SCRIPT.read_text(encoding="utf-8")

        self.assertIn('module_arr_=(op_ut_run op_ut_helper msopst msopst.ini optype_collector)', content)
        self.assertIn('whlUninstallPackage optype_collector ${install_path}/python/site-packages', content)
        self.assertIn('removeSoftLink "${install_path}/${PLT_ARCH}-linux/bin/" "msopgen"', content)
        self.assertIn('removeSoftLink "${install_path}/${PLT_ARCH}-linux/bin/" "msopst"', content)
        self.assertIn('removeSoftLink "${install_path}/${PLT_ARCH}-linux/bin/" "optype_collector"', content)
        self.assertNotIn('removeSoftLink "${install_path}/bin" "optype_collector"', content)

    def test_docs_describe_usage_without_exposing_install_or_internal_soc_details(self):
        user_doc = USER_DOC.read_text(encoding="utf-8")

        self.assertIn("optype_collector {soc_version}", user_doc)
        self.assertIn("optype_collector --detect-conflicts {soc_version}", user_doc)
        self.assertIn("用户无需进入工具目录", user_doc)
        self.assertNotIn("${install_path}", user_doc)
        self.assertNotIn("${install_path}/bin/optype_collector", user_doc)
        self.assertNotIn("${install_path}/python/site-packages/bin/optype_collector", user_doc)
        self.assertNotIn("tools/optype_collector/optype_collector", user_doc)
        self.assertNotIn("msopgen", user_doc)
        self.assertNotIn("msopst", user_doc)
        self.assertNotIn("ascend910_95", user_doc)
        self.assertNotIn("兼容", user_doc)


if __name__ == "__main__":
    unittest.main()
