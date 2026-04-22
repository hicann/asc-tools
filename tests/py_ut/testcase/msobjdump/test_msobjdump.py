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
import contextlib

from unittest.mock import MagicMock, patch
from io import StringIO

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
        self._clean_out_dir(out_dir)


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
        self._clean_out_dir(out_dir)

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
        self._clean_out_dir(out_dir)

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
        self._clean_out_dir(out_dir)

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
        self._clean_out_dir(out_dir)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.extract_aicore_binary_from_elf')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_symbols_in_file')
    def test_dump_elf_fusion_compile(self, mock_symbol, mock_section, mock_all, mock_run, mock_elf_content):
        mock_symbol.side_effect = ['test section list \n', 'test section list \n']
        mock_all.return_value = 'test all section and symbols \n'
        mock_section.side_effect = [
            '[26] .aicore_binary    PROGBITS        00000000000ab500 07b500 0072c0 00  WA  0   0 256\n',
            '[15] .ascend.meta.gen_FFN_2000_mix_aiv NOTE            0000000000000000 06e108 000010 00      0   0  4\n',
            '[15] .ascend.meta.gen_FFN_2000_mix_aiv NOTE            0000000000000000 06e108 000010 00      0   0  4\n'
        ]
        mock_elf_content.return_value = b'\x01\x00\x04\x00\x04\x00\x00\x00\x03\x00\x04\x00\x01\x00\x01\x00'

        def _mock_objcopy(input_file, output_file):
            with open(output_file, 'wb') as f:
                f.write(b'fusion_meta')
            result = MagicMock()
            result.returncode = 0
            result.stderr = ''
            return result

        mock_run.side_effect = _mock_objcopy
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_dump_elf_fusion_compile')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_dump_elf_fusion_compile')
        parse_mock.dump_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.list_elf = None
        parse_mock.verbose = True
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError:
            self.assertTrue(False)
        else:
            self.assertTrue(mock_run.called)
            self.assertTrue(mock_all.called)
        self._clean_out_dir(out_dir)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.extract_aicore_binary_from_elf')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_symbols_in_file')
    def test_list_elf_fusion_compile(self, mock_symbol, mock_section, mock_all, mock_run, mock_elf_content):
        mock_symbol.side_effect = ['test section list \n', 'test section list \n']
        mock_all.return_value = 'test all section and symbols \n'
        kernel_section = \
            '[23] .ascend.kernel.ascend910b1.ascendc_kernels_npu PROGBITS   0000  0002  018 00  WA  0   0  8\n'
        mock_section.side_effect = [
            '[26] .aicore_binary    PROGBITS        00000000000ab500 07b500 0072c0 00  WA  0   0 256\n',
            kernel_section,
            kernel_section
        ]
        mock_elf_content.return_value = b'\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00'

        def _mock_objcopy(input_file, output_file):
            with open(output_file, 'wb') as f:
                f.write(b'fusion_kernel')
            result = MagicMock()
            result.returncode = 0
            result.stderr = ''
            return result

        mock_run.side_effect = _mock_objcopy
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_list_elf_fusion_compile')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_list_elf_fusion_compile')
        parse_mock.list_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.dump_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError:
            self.assertTrue(False)
        else:
            self.assertTrue(mock_run.called)
        self._clean_out_dir(out_dir)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.extract_aicore_binary_from_elf')
    @patch('msobjdump.utils.get_all_section_symbols_in_file')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_symbols_in_file')
    def test_extract_elf_fusion_compile(self, mock_symbol, mock_section, mock_all, mock_run, mock_elf_content):
        mock_symbol.side_effect = ['test section list \n', 'test section list \n']
        mock_all.return_value = 'test all section and symbols \n'
        kernel_section = \
            '[23] .ascend.kernel.ascend910b1.ascendc_kernels_npu PROGBITS   0000  0002  018 00  WA  0   0  8\n'
        mock_section.side_effect = [
            '[26] .aicore_binary    PROGBITS        00000000000ab500 07b500 0072c0 00  WA  0   0 256\n',
            kernel_section,
            kernel_section
        ]
        mock_elf_content.return_value = b'\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00'

        def _mock_objcopy(input_file, output_file):
            with open(output_file, 'wb') as f:
                f.write(b'fusion_kernel')
            result = MagicMock()
            result.returncode = 0
            result.stderr = ''
            return result

        mock_run.side_effect = _mock_objcopy
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_extract_elf_fusion_compile')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_extract_elf_fusion_compile')
        parse_mock.extr_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.list_elf = None
        parse_mock.dump_elf = None
        try:
            msobjdump_main.run_obj_dump(parse_mock)
        except RuntimeError:
            self.assertTrue(False)
        else:
            self.assertTrue(mock_run.called)
        self.assertTrue(os.path.exists(os.path.join(out_dir, 'test_extract_elf_fusion_compile.aicore.o')))
        self._clean_out_dir(out_dir)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.extract_aicore_binary_from_elf')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_symbols_in_file')
    def test_list_elf_fusion_compile_meta_lists_extracted_binary(self, mock_symbol, mock_section, mock_run, mock_elf_content):
        mock_symbol.side_effect = ['test section list \n', 'test section list \n']
        mock_section.side_effect = [
            '[26] .aicore_binary    PROGBITS        00000000000ab500 07b500 0072c0 00  WA  0   0 256\n',
            '[15] .ascend.meta.gen_FFN_2000_mix_aiv NOTE            0000000000000000 06e108 000010 00      0   0  4\n'
        ]
        mock_elf_content.return_value = b'\x01\x00\x04\x00\x04\x00\x00\x00\x03\x00\x04\x00\x01\x00\x01\x00'

        def _mock_objcopy(input_file, output_file):
            with open(output_file, 'wb') as f:
                f.write(b'fusion_meta')
            result = MagicMock()
            result.returncode = 0
            result.stderr = ''
            return result

        mock_run.side_effect = _mock_objcopy
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_list_elf_fusion_compile_meta')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_list_elf_fusion_compile_meta')
        parse_mock.list_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.dump_elf = None

        captured_output = StringIO()
        with contextlib.redirect_stdout(captured_output):
            msobjdump_main.run_obj_dump(parse_mock)
        self.assertIn('ELF file    0: test_list_elf_fusion_compile_meta.aicore.o', captured_output.getvalue())
        self._clean_out_dir(out_dir)

    @patch('msobjdump.msobjdump_main.ObjDump._get_segment_content')
    @patch('msobjdump.utils.extract_aicore_binary_from_elf')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_symbols_in_file')
    def test_extract_elf_fusion_compile_meta_outputs_extracted_binary(self, mock_symbol, mock_section, mock_run, mock_elf_content):
        mock_symbol.side_effect = ['test section list \n', 'test section list \n']
        mock_section.side_effect = [
            '[26] .aicore_binary    PROGBITS        00000000000ab500 07b500 0072c0 00  WA  0   0 256\n',
            '[15] .ascend.meta.gen_FFN_2000_mix_aiv NOTE            0000000000000000 06e108 000010 00      0   0  4\n'
        ]
        mock_elf_content.return_value = b'\x01\x00\x04\x00\x04\x00\x00\x00\x03\x00\x04\x00\x01\x00\x01\x00'

        def _mock_objcopy(input_file, output_file):
            with open(output_file, 'wb') as f:
                f.write(b'fusion_meta')
            result = MagicMock()
            result.returncode = 0
            result.stderr = ''
            return result

        mock_run.side_effect = _mock_objcopy
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_extract_elf_fusion_compile_meta')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_extract_elf_fusion_compile_meta')
        parse_mock.extr_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.list_elf = None
        parse_mock.dump_elf = None

        msobjdump_main.run_obj_dump(parse_mock)
        self.assertTrue(os.path.exists(os.path.join(out_dir, 'test_extract_elf_fusion_compile_meta.aicore.o')))
        self._clean_out_dir(out_dir)

    @patch('msobjdump.utils.extract_aicore_binary_from_elf')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_symbols_in_file')
    def test_fusion_compile_objcopy_failed(self, mock_symbol, mock_section, mock_run):
        mock_symbol.return_value = 'test section list \n'
        mock_section.return_value = \
            '[26] .aicore_binary    PROGBITS        00000000000ab500 07b500 0072c0 00  WA  0   0 256\n'
        result = MagicMock()
        result.returncode = 1
        result.stderr = 'mock objcopy failed'
        mock_run.return_value = result

        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_fusion_compile_objcopy_failed')
        parse_mock.out_dir = out_dir
        elf_file = os.path.join(out_dir, 'test_fusion_compile_objcopy_failed')
        parse_mock.dump_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.list_elf = None

        with self.assertRaises(RuntimeError):
            msobjdump_main.run_obj_dump(parse_mock)
        self._clean_out_dir(out_dir)

    @patch('msobjdump.utils.extract_aicore_binary_from_elf')
    @patch('msobjdump.utils.get_section_headers_in_file')
    @patch('msobjdump.utils.get_symbols_in_file')
    def test_fusion_compile_unsupported_after_extract(self, mock_symbol, mock_section, mock_run):
        mock_symbol.side_effect = ['test section list \n', 'test section list \n']
        mock_section.side_effect = [
            '[26] .aicore_binary    PROGBITS        00000000000ab500 07b500 0072c0 00  WA  0   0 256\n',
            '[26] .aicore_binary    PROGBITS        00000000000ab500 07b500 0072c0 00  WA  0   0 256\n'
        ]

        def _mock_objcopy(input_file, output_file):
            with open(output_file, 'wb') as f:
                f.write(b'fusion_unknown')
            result = MagicMock()
            result.returncode = 0
            result.stderr = ''
            return result

        mock_run.side_effect = _mock_objcopy
        parse_mock = MagicMock()
        out_dir = self._make_out_dir('test_fusion_compile_unsupported_after_extract')
        parse_mock.out_dir = out_dir
        parse_mock.verbose = False
        elf_file = os.path.join(out_dir, 'test_fusion_compile_unsupported_after_extract')
        parse_mock.dump_elf = elf_file
        with open(elf_file, 'a+') as f:
            f.write('test')
        parse_mock.extr_elf = None
        parse_mock.list_elf = None

        msobjdump_main.run_obj_dump(parse_mock)
        self._clean_out_dir(out_dir)

    def test_extract_aicore_binary_edge_cases(self):
        with self.subTest("remove old tmp and extract success"):
            objdump = msobjdump_main.ObjDump.__new__(msobjdump_main.ObjDump)
            objdump.tmp_dir = self._make_out_dir('test_extract_aicore_binary_edge_success')
            objdump.obj = os.path.join(objdump.tmp_dir, 'input_bin')
            with patch('msobjdump.msobjdump_main.os.path.exists', side_effect=[True, True]) as mock_exists, \
                 patch('msobjdump.msobjdump_main.os.path.getsize', return_value=8), \
                 patch('msobjdump.msobjdump_main.os.remove') as mock_remove, \
                 patch('msobjdump.msobjdump_main.utils.extract_aicore_binary_from_elf') as mock_run:
                result = MagicMock()
                result.returncode = 0
                result.stderr = ''
                mock_run.return_value = result
                tmp_file = objdump._extract_aicore_binary()
                self.assertTrue(mock_exists.called)
                self.assertTrue(mock_remove.called)
                self.assertEqual(tmp_file, os.path.join(objdump.tmp_dir, 'fusion_aicore_binary.aicore.o'))
            shutil.rmtree(objdump.tmp_dir, ignore_errors=True)

        with self.subTest("llvm-objcopy not found"):
            objdump = msobjdump_main.ObjDump.__new__(msobjdump_main.ObjDump)
            objdump.tmp_dir = self._make_out_dir('test_extract_aicore_binary_edge_not_found')
            objdump.obj = os.path.join(objdump.tmp_dir, 'input_bin')
            with patch('msobjdump.msobjdump_main.os.path.exists', return_value=False), \
                 patch('msobjdump.msobjdump_main.utils.extract_aicore_binary_from_elf',
                       side_effect=FileNotFoundError('llvm-objcopy not found')):
                with self.assertRaises(RuntimeError) as ctx:
                    objdump._extract_aicore_binary()
                self.assertIn('llvm-objcopy is not available', str(ctx.exception))
            shutil.rmtree(objdump.tmp_dir, ignore_errors=True)

        with self.subTest("extract output missing or empty"):
            objdump = msobjdump_main.ObjDump.__new__(msobjdump_main.ObjDump)
            objdump.tmp_dir = self._make_out_dir('test_extract_aicore_binary_edge_empty')
            objdump.obj = os.path.join(objdump.tmp_dir, 'input_bin')
            with patch('msobjdump.msobjdump_main.os.path.exists', side_effect=[False, False]), \
                 patch('msobjdump.msobjdump_main.utils.extract_aicore_binary_from_elf') as mock_run:
                result = MagicMock()
                result.returncode = 0
                result.stderr = ''
                mock_run.return_value = result
                with self.assertRaises(RuntimeError) as ctx:
                    objdump._extract_aicore_binary()
                self.assertIn('file is empty or missing', str(ctx.exception))
            shutil.rmtree(objdump.tmp_dir, ignore_errors=True)

    def test_show_ascend_meta_tlv_block_num(self):
        content = b'\xff\xff\xff\xff'
        t = 15
        l = 4
        index = 0

        captured_output = StringIO()
        sys.stdout = captured_output

        msobjdump_main.ObjDump._show_ascend_meta_tlv(content, t, l, index)

        sys.stdout = sys.__stdout__
        output = captured_output.getvalue().strip()

        self.assertEqual(output, "BLOCK_NUM: 0xFFFFFFFF")

    def test_show_ascend_meta_op_tlv_dedup_for_aicore_binary(self):
        objdump = msobjdump_main.ObjDump.__new__(msobjdump_main.ObjDump)
        objdump.obj_type = msobjdump_main.ObjType.TYPE_AICORE_BINARY
        objdump._aicore_binary_meta_printed = set()
        version_content = b'\x01\x00\x00\x00'
        runtime_content = b'\x03\x00\x00\x00'

        captured_output = StringIO()
        sys.stdout = captured_output

        objdump._show_ascend_meta_op_tlv(
            runtime_content,
            msobjdump_main.B_TYPE_RUNTIME_IMPLICIT_INFO,
            4,
            0
        )
        objdump._show_ascend_meta_op_tlv(version_content, msobjdump_main.B_TYPE_VERSION, 4, 0)
        objdump._show_ascend_meta_op_tlv(version_content, msobjdump_main.B_TYPE_VERSION, 4, 0)

        sys.stdout = sys.__stdout__
        output = captured_output.getvalue().strip().splitlines()

        self.assertEqual(output, ["RUNTIME_IMPLICIT_INFO: L2Cache Hint Flag", "VERSION: 1"])

    def test_show_ascend_meta_op_tlv_other_types(self):
        objdump = msobjdump_main.ObjDump.__new__(msobjdump_main.ObjDump)
        objdump.obj_type = msobjdump_main.ObjType.TYPE_ASCEND_META
        objdump._aicore_binary_meta_printed = set()

        captured_output = StringIO()
        sys.stdout = captured_output

        objdump._show_ascend_meta_op_tlv(
            b'\x08\x00\x00\x00\x02\x00\x00\x00',
            msobjdump_main.B_TYPE_DEBUG,
            8,
            0
        )
        objdump._show_ascend_meta_op_tlv(
            b'\x00\x00\x03\x00',
            msobjdump_main.B_TYPE_DYNAMIC_PARAM,
            4,
            0
        )
        objdump._show_ascend_meta_op_tlv(
            b'\x01\x00\x02\x00',
            msobjdump_main.B_TYPE_OPTIONAL_PARAM,
            4,
            0
        )
        objdump._show_ascend_meta_op_tlv(
            b'\x03\x00\x00\x00',
            msobjdump_main.B_TYPE_RUNTIME_IMPLICIT_INFO,
            4,
            0
        )

        sys.stdout = sys.__stdout__
        output = captured_output.getvalue().strip().splitlines()

        self.assertEqual(
            output,
            [
                "DEBUG: debugBufSize=8, debugOptions=2",
                "DYNAMIC_PARAM: dynamicParamMode=3",
                "OPTIONAL_PARAM: optionalInputMode=1, optionalOutputMode=2",
                "RUNTIME_IMPLICIT_INFO: L2Cache Hint Flag"
            ]
        )

    def test_utils_helper_functions(self):
        self.assertEqual(utils.split_str_with_space("  foo\t bar   baz  "), ["foo", "bar", "baz"])
        self.assertEqual(utils.get_str_between("_binary_demo_o_start", "_binary_", "_start"), "demo_o")
        self.assertEqual(utils.get_str_between("abc", "_binary_", "_start"), "")
        self.assertTrue(utils.is_prefix_substring("Ascend910", ["config", "ascend"]))
        self.assertFalse(utils.is_prefix_substring("kernel", ["config", "ascend"]))

    @patch('msobjdump.utils.subprocess.run')
    @patch('msobjdump.utils.shutil.copy')
    @patch('msobjdump.utils.os.makedirs')
    @patch('msobjdump.utils.os.path.exists')
    def test_utils_file_and_command_wrappers(self, mock_exists, mock_makedirs, mock_copy, mock_run):
        mock_exists.side_effect = [True, False]
        utils.copy_file_src_exist("/tmp/src.txt", "/tmp/out/dest.txt")
        mock_makedirs.assert_called_once_with("/tmp/out")
        mock_copy.assert_called_once_with("/tmp/src.txt", "/tmp/out/dest.txt")

        mock_exists.reset_mock()
        mock_makedirs.reset_mock()
        mock_copy.reset_mock()
        mock_exists.side_effect = None
        mock_exists.return_value = False
        utils.copy_file_src_exist("/tmp/missing.txt", "/tmp/out/dest.txt")
        mock_makedirs.assert_not_called()
        mock_copy.assert_not_called()

        mock_run.return_value = MagicMock(stdout="test output")
        self.assertEqual(utils.get_section_headers_in_file("test.o"), "test output")
        self.assertEqual(utils.get_symbols_in_file("test.o"), "test output")
        self.assertEqual(utils.get_all_section_symbols_in_file("test.o"), "test output")

        mock_result = MagicMock()
        mock_run.return_value = mock_result
        self.assertIs(utils.get_o_file_from_a_file("libtest.a", "demo.o"), mock_result)
        self.assertIs(utils.extract_aicore_binary_from_elf("demo", "demo.aicore.o"), mock_result)


if __name__ == "__main__":
    unittest.main()
