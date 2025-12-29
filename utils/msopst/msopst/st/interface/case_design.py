#!/usr/bin/env python
# coding=utf-8
"""
Function:
CaseDesign class
This class mainly involves parse
# -------------------------------------------------------------------------
# This file is part of the MindStudio project.
# Copyright (c) 2025 Huawei Technologies Co.,Ltd.
#
# MindStudio is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#
#          http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
# -------------------------------------------------------------------------

"""


import os

from msopst.st.interface import utils
from msopst.st.interface.subcase_design_fuzz import SubCaseDesignFuzz
from msopst.st.interface.subcase_design_cross import SubCaseDesignCross
from msopst.st.interface.const_manager import ConstManager


class CaseDesign:
    """
    the class for design test case.
    """

    def __init__(self, json_path_list, case_name_list, report):
        self.json_path_list = json_path_list.split(',')
        if case_name_list == 'all':
            self.case_name_list = None
        else:
            self.case_name_list = case_name_list.split(',')
        self.current_json_path = ''
        self.case_name_to_json_file_map = {}
        self.report = report

    def check_argument_valid(self):
        """
        check input json file valid
        """
        json_file_list = []
        for json_path in self.json_path_list:
            json_path = os.path.realpath(json_path)
            if not json_path.endswith(".json"):
                utils.print_error_log(
                    'The file "%s" is invalid, only supports .json file. '
                    'Please modify it.' % json_path)
                raise utils.OpTestGenException(
                    ConstManager.OP_TEST_GEN_INVALID_PATH_ERROR)
            utils.check_path_valid(json_path)
            json_file_list.append(json_path)
        self.json_path_list = json_file_list

    def generate_cases(self):
        """
        Generate test case by json file
        :return: the list of test case
        """
        total_case_in_file = []
        compile_flag = None
        for json_path in self.json_path_list:
            utils.print_info_log('Start to create sub test cases for %s.'
                                 % json_path)
            self.current_json_path = json_path
            # load json file
            json_object = utils.load_json_file(json_path)
            # parse json object
            for json_obj in json_object:
                if json_obj.get("compile_flag"):
                    compile_flag = json_obj.get("compile_flag")
                    continue
                utils.check_required_key_valid(json_obj, ConstManager.REQUIRED_KEYS, 'case',
                                         self.current_json_path)
                # skip the case name not in case_name_list
                if self.case_name_list and \
                        json_obj[ConstManager.CASE_NAME] not in self.case_name_list:
                    continue
                total_case_in_file = self._get_total_case(json_obj, json_path,
                                                          total_case_in_file)
        return total_case_in_file, compile_flag

    def design(self):
        """
        Design test case by json file.
        :return: the test case list
        """
        # check json path valid
        self.check_argument_valid()

        # design sub test case by json file
        utils.print_step_log("[%s] Start to parser testcase json." % (os.path.basename(__file__)))
        case_list = self.generate_cases()

        if len(case_list[0]) == 0:
            case_info = 'all'
            if self.case_name_list:
                case_info = str(self.case_name_list)
            utils.print_error_log(
                'There is no case to generate for %s. Please modify the case '
                'name argument.' % case_info)
            raise utils.OpTestGenException(
                ConstManager.OP_TEST_GEN_INVALID_DATA_ERROR)
        return case_list

    def _get_total_case(self, json_obj, json_path, total_case_in_file):
        if json_obj[ConstManager.CASE_NAME] in self.case_name_to_json_file_map:
            utils.print_error_log(
                'The case name "%s" already exists. Please modify or '
                'remove the redundant case name in file %s.'
                % (json_obj[ConstManager.CASE_NAME], self.current_json_path))
            raise utils.OpTestGenException(
                ConstManager.OP_TEST_GEN_INVALID_DATA_ERROR)
        self.case_name_to_json_file_map[
            json_obj[ConstManager.CASE_NAME]] = json_path

        if json_obj.get(ConstManager.FUZZ_IMPL):
            subcase_parse = SubCaseDesignFuzz(self.current_json_path,
                                              json_obj,
                                              total_case_in_file,
                                              self.report)
        else:
            subcase_parse = SubCaseDesignCross(self.current_json_path,
                                               json_obj,
                                               total_case_in_file,
                                               self.report)
        total_case_in_file = subcase_parse.subcase_generate()
        return total_case_in_file
