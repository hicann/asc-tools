#! /usr/bin/env python3
# -*- coding: UTF-8 -*-
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
import os
import sys
import shutil
import unittest
import subprocess

from unittest.mock import MagicMock, patch

THIS_FILE_NAME = __file__
FILE_PATH = os.path.dirname(os.path.realpath(THIS_FILE_NAME))
MSOBJDUMP_PATH = os.path.join(FILE_PATH, "../../../../", "utils/msobjdump")
print(MSOBJDUMP_PATH)
sys.path = [MSOBJDUMP_PATH] + sys.path
from msobjdump import msobjdump_main, utils


class TestMsObjdump(unittest.TestCase):
    def setUp(self):
        # 每个测试用例执行之前做操作
        print("---------------------set up case----------------------------------")

    def tearDown(self):
        # 每个测试用例执行之后做操作
        print("---------------------tear down case-------------------------------")

    def _make_out_dir(self, out_dir_name):
        out_dir = os.path.join(FILE_PATH, out_dir_name)
        if not os.path.exists(out_dir):
            os.mkdir(out_dir)
        return out_dir

    def _clean_out_dir(self, out_dir_name):
        out_dir = os.path.join(FILE_PATH, out_dir_name)
        if os.path.exists(out_dir):
            shutil.rmtree(out_dir)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.get_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    def test_dump_elf_aclnn(self, mock_all, mock_section, mock_symbol, mock_elf_content):
        mock_symbol.return_value = \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_start\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_end\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_size\n'
        mock_all.return_value = \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_start\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_end\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_size\n'
        mock_section.return_value = \
            '[21] .dynamic          DYNAMIC         00000000000dff10 0def10 000260 10  WA  4   0  8\n' \
            '[22] .got              PROGBITS        00000000000e0170 0df170 001e90 08  WA  0   0  8\n' \
            '[23] .data             PROGBITS        00000000000e2000 0e1000 0bb493 00  WA  0   0 32\n' \
            '[24] .bss              NOBITS          000000000019d4a0 19c493 005ed0 00  WA  0   0 32\n' \
            '[25] .comment          PROGBITS        0000000000000000 19c493 000045 01  MS  0   0  1'
        mock_elf_content.return_value = bytes('{"test_key": "test_content"}\n', 'utf-8')
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_dump_elf_aclnn')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_dump_elf_aclnn.o')
        parse_mock.dump_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.list_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError as e:
            print(f'runtime error: {str(e)}')
            self.assertTrue(False)
        else:
            print("test run dump elf without exception")
            self.assertTrue(True)


    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.get_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    def test_list_elf_aclnn(self, mock_all, mock_section, mock_symbol, mock_elf_content):
        mock_symbol.return_value = \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_start\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_end\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_size\n'
        mock_all.return_value = \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_start\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_end\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_size\n'
        mock_section.return_value = \
            '[21] .dynamic          DYNAMIC         00000000000dff10 0def10 000260 10  WA  4   0  8\n' \
            '[22] .got              PROGBITS        00000000000e0170 0df170 001e90 08  WA  0   0  8\n' \
            '[23] .data             PROGBITS        00000000000e2000 0e1000 0bb493 00  WA  0   0 32\n' \
            '[24] .bss              NOBITS          000000000019d4a0 19c493 005ed0 00  WA  0   0 32\n' \
            '[25] .comment          PROGBITS        0000000000000000 19c493 000045 01  MS  0   0  1'
        mock_elf_content.return_value = bytes('{"test_key": "test_content"}\n', 'utf-8')
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_list_elf_aclnn')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_list_elf_aclnn.o')
        parse_mock.list_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.dump_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError as e:
            self.assertTrue(False)
        else:
            print("test run dump elf without exception")
            self.assertTrue(True)
        self._clean_out_dir(out_dir)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.get_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    def test_extr_elf_aclnn(self, mock_all, mock_section, mock_symbol, mock_elf_content):
        mock_symbol.return_value = \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_start\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_end\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_size\n'
        mock_all.return_value = \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_start\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_end\n' \
            '7824: 000000000010c0a0     0 NOTYPE  GLOBAL DEFAULT   23 _binary_ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4_o_size\n'
        mock_section.return_value = \
            '[21] .dynamic          DYNAMIC         00000000000dff10 0def10 000260 10  WA  4   0  8\n' \
            '[22] .got              PROGBITS        00000000000e0170 0df170 001e90 08  WA  0   0  8\n' \
            '[23] .data             PROGBITS        00000000000e2000 0e1000 0bb493 00  WA  0   0 32\n' \
            '[24] .bss              NOBITS          000000000019d4a0 19c493 005ed0 00  WA  0   0 32\n' \
            '[25] .comment          PROGBITS        0000000000000000 19c493 000045 01  MS  0   0  1'
        mock_elf_content.return_value = bytes('{"test_key": "test_content"}\n', 'utf-8')
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_extr_elf_aclnn')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_extr_elf_aclnn')
        parse_mock.extr_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.list_elf = None
        parse_mock.dump_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError as e:
            self.assertTrue(False)
            print("test file not exit")
        else:
            print("test run dump elf without exception")
            self.assertTrue(True)
        self.assertTrue(os.path.exists(os.path.join(out_dir, 'ascend910b_axpy_custom_AxpyCustom_1b6d4d1dfddfa974458c4fc379248bc4.o')))
        self._clean_out_dir(out_dir)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.get_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    def test_dump_elf_ascend_kernel(self, mock_all, mock_section, mock_symbol, mock_elf_content):
        mock_symbol.return_value = 'test section list \n'
        mock_all.return_value = 'test all section and symbols \n'
        mock_section.return_value = \
            '[22] .data             PROGBITS        000000000003f000 02f000 000010 00  WA  0   0  8\n' \
            '[23] .ascend.kernel.ascend910b1.ascendc_kernels_npu PROGBITS   0000  0002  004  00  WA  0   0  8\n' \
            '[24] .bss              NOBITS          00000000000bbe70 0abe6c 000128 00  WA  0   0  8\n'
        mock_elf_content.return_value = b'\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x00\x00'
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_dump_elf_ascend_kernel')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_dump_elf_ascend_kernel.o')
        parse_mock.dump_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.list_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError as e:
            print(f'runtime error: {str(e)}')
            self.assertTrue(False)
        else:
            print("test run dump elf without exception")
            self.assertTrue(True)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.get_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    def test_list_elf_ascend_kernel(self, mock_all, mock_section, mock_symbol, mock_elf_content):
        mock_symbol.return_value = 'test section list \n'
        mock_all.return_value = 'test all section and symbols \n'
        mock_section.return_value = \
            '[22] .data             PROGBITS        000000000003f000 02f000 000010 00  WA  0   0  8\n' \
            '[23] .ascend.kernel.ascend910b1.ascendc_kernels_npu PROGBITS   0000  0002  004 00  WA  0   0  8\n' \
            '[24] .bss              NOBITS          00000000000bbe70 0abe6c 000128 00  WA  0   0  8\n'
        mock_elf_content.return_value = b'\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x00\x00'
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_list_elf_ascend_kernel')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_list_elf_ascend_kernel.o')
        parse_mock.list_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.dump_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError as e:
            self.assertTrue(False)
        else:
            print("test run dump elf without exception")
            self.assertTrue(True)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.get_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    def test_extr_elf_ascend_kernel(self, mock_all, mock_section, mock_symbol, mock_elf_content):
        mock_symbol.return_value = 'test section list \n'
        mock_all.return_value = 'test all section and symbols \n'
        mock_section.return_value = \
            '[22] .data             PROGBITS        000000000003f000 02f000 000010 00  WA  0   0  8\n' \
            '[23] .ascend.kernel.ascend910b1.ascendc_kernels_npu PROGBITS        0000  0002  004 00  WA  0   0  8\n' \
            '[24] .bss              NOBITS          00000000000bbe70 0abe6c 000128 00  WA  0   0  8\n'
        mock_elf_content.return_value = b'\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x01\x00\x00\x00\x00\x00'
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_extr_elf_ascend_kernel')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_extr_elf_ascend_kernel')
        parse_mock.extr_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.list_elf = None
        parse_mock.dump_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError as e:
            self.assertTrue(False)
            print("test file not exit")
        else:
            print("test run dump elf without exception")
            self.assertTrue(True)
        self.assertTrue(os.path.exists(os.path.join(out_dir, 'ascend910b1_ascendc_kernels_npu_0_mix.o')))

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.get_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    def test_dump_elf_ascend_meta(self, mock_all, mock_section, mock_symbol, mock_elf_content):
        mock_symbol.return_value = 'test section list \n'
        mock_all.return_value = 'test all section and symbols \n'
        mock_section.return_value = \
            '[14] .debug_line_str   PROGBITS        0000000000000000 06ca46 0016c1 01  MS  0   0  1\n'\
            '[15] .ascend.meta.gen_FFN_2000_mix_aiv NOTE            0000000000000000 06e108 000010 00      0   0  4\n' \
            '[16] .bl_uninit        NOBITS          0000000000000000 06e118 000028 00      0   0  1\n'
        mock_elf_content.return_value = b'\x01\x00\x04\x00\x04\x00\x00\x00\x03\x00\x04\x00\x01\x00\x01\x00'
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_dump_elf_ascend_meta')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_dump_elf_ascend_meta.o')
        parse_mock.dump_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.list_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError as e:
            print(f'runtime error: {str(e)}')
            self.assertTrue(False)
        else:
            print("test run dump elf without exception")
            self.assertTrue(True)


if __name__ == "__main__":
    unittest.main()
