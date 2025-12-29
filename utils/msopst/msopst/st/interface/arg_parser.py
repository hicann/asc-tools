#!/usr/bin/env python
# coding=utf-8
"""
Function:
This file mainly involves class for parsing input arguments.
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
import ast
import os
import sys
import argparse
import time

from msopst.st.interface import utils
from msopst.st.interface.const_manager import ConstManager


class MsopstArgParser:
    """
    class MsopstArgParser
    """
    def __init__(self):
        # parse input argument
        parse = argparse.ArgumentParser()
        subparsers = parse.add_subparsers(help='commands')
        create_parser = subparsers.add_parser(
            'create', help='Create test case json file.', allow_abbrev=False)
        run_parser = subparsers.add_parser(
            'run', help='Run the test case on the aihost.', allow_abbrev=False)
        gen_ascendc_parser = subparsers.add_parser(
            'ascendc_test', help='Generate test code to call of kernel function for AscendC operators.',
            allow_abbrev=False)
        if len(sys.argv) <= 1:
            parse.print_usage()
            raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_PARAM_ERROR)
        self._create_parser(create_parser)
        self._run_parser(run_parser)
        self._gen_ascendc_parser(gen_ascendc_parser)
        self.input_file = ""
        self.output_path = ""
        self.case_name = ''
        self.model_path = ''
        self.device_id = 0
        self.soc_version = ''
        self.err_thr = ''
        self.config_file = ''
        self.report_path = ''
        self.expect_path = ''
        self.result_path = ''
        self.error_report = ''
        args = parse.parse_args(sys.argv[1:])
        if sys.argv[1] == 'create':
            self._check_create_args(args)
        elif sys.argv[1] == 'ascendc_test':
            self._check_gen_ascendc_test_args(args)
        else:
            self._check_run_args(args)

    @staticmethod
    def _add_time_steamp(out_path):
        time_dir = time.strftime("%Y%m%d%H%M%S", time.localtime())
        return os.path.realpath(os.path.join(out_path, time_dir))

    @staticmethod
    def _create_parser(create_parser):
        """
        parse create cmd
        :param create_parser:
        """
        create_parser.add_argument(
            "-i", "--input", dest="input_file", default="",
            help="<Required> the input file, .ini or .py or .cpp file", required=True)
        create_parser.add_argument(
            "-out", "--output", dest="output_path", default="",
            help="<Optional> the output path", required=False)
        create_parser.add_argument(
            "-m", "--model", dest="model_path", default="",
            help="<Optional> the model path", required=False)
        create_parser.add_argument(
            '-q', "--quiet", dest="quiet", action="store_true", default=False,
            help="<Optional> quiet mode, skip human-computer interactions",
            required=False)

    @staticmethod
    def _run_parser(run_parser):
        """
        parse run cmd
        :param run_parser:
        """
        run_parser.add_argument(
            "-i", "--input", dest="input_file", default="",
            help="<Required> the input file, .json file, ", required=True)
        run_parser.add_argument(
            '-soc', "--soc_version", dest="soc_version",
            help="<Required> the soc version to run", required=True)
        run_parser.add_argument(
            "-out", "--output", dest="output_path", default="",
            help="<Optional> the output path", required=False)
        run_parser.add_argument(
            '-c', "--case_name", dest="case_name", default='all',
            help="<Optional> the case name to run or gen, splits with ',', "
                 "like 'case0,case1'.", required=False)
        run_parser.add_argument(
            '-d', "--device_id", dest="device_id", default="0",
            help="<Optional> input device id, default is 0.",
            required=False)
        run_parser.add_argument(
            '-conf', "--config_file", dest="config_file", default="",
            help="<Optional> config_file, msopst advance config file.",
            required=False)
        run_parser.add_argument(
            '-err_thr', "--error_threshold", dest="error_threshold",
            help="<Optional> error_threshold, Error threshold of result"
                 "comparison, like [0.001, 0.001].",
            required=False)
        run_parser.add_argument(
            '-err_report', "--error_report", dest="error_report",
            default="false", choices=['false', 'true'],
            help="<Optional> Generate error reports (.csv) for failed ST cases. "
                 "This option is available when the script for expected result verification is specified.",
            required=False)

    @staticmethod
    def _gen_ascendc_parser(gen_ascendc_parser):
        """
        parse run cmd
        :param _run_ascendc_parser:
        """
        gen_ascendc_parser.add_argument(
            "-i", "--input", dest="input_file",
            help="<Required> the input file, *.json file, ", required=True)
        gen_ascendc_parser.add_argument(
            "-kernel", "--kernel_file", dest="kernel_file",
            help="<Required> the kernel file for AscendC operators.", required=True)
        gen_ascendc_parser.add_argument(
            "-out", "--output", dest="output_path", default='',
            help="<Optional> the output path", required=False)

    @staticmethod
    def _check_file_valid(input_file, isdir=False):
        utils.check_path_valid(input_file, isdir)
        return input_file
    
    @staticmethod
    def _check_path_permission_valid(input_file):
        utils.check_input_permission_valid(input_file)
        dir_path = os.path.dirname(os.path.realpath(input_file))
        utils.check_input_permission_valid(dir_path)

    def get_input_file(self):
        """
        get input file
        :return: input file
        """
        return self.input_file

    def get_output_path(self):
        """
        get output path
        :return: output path
        """
        return self.output_path

    def _check_gen_ascendc_test_args(self, args):
        self.input_file = self._check_file_valid(args.input_file)
        self._check_path_permission_valid(self.input_file)
        self.kernel_file = self._check_file_valid(args.kernel_file)
        if args.output_path:
            utils.check_path_valid(args.output_path, isdir=True)
        self.output_path = self._add_time_steamp(args.output_path)

    def _check_create_args(self, args):
        self.input_file = args.input_file
        self.model_path = args.model_path
        self.quiet = args.quiet
        self.output_path = args.output_path
        utils.check_path_valid(self.input_file)
        self._check_path_permission_valid(self.input_file)
        if self.output_path:
            utils.check_path_valid(self.output_path, isdir=True)
        self._check_create_model_quiet()

    def _check_run_args(self, args):
        self.input_file = self._check_file_valid(args.input_file)
        self._check_path_permission_valid(self.input_file)
        self._check_case_name_valid(args.case_name)
        self._check_soc_version(args.soc_version)
        self._check_device_id(args.device_id)
        self._gen_error_threshold(args.error_threshold)
        self.error_report = args.error_report
        if args.config_file != "":
            utils.check_path_valid(args.config_file)
        self.config_file = args.config_file
        if args.output_path:
            utils.check_path_valid(args.output_path, isdir=True)
        self.output_path = self._add_time_steamp(args.output_path)

    def _check_case_name_valid(self, case_name):
        if case_name != '':
            utils.check_name_valid(case_name, name_type="case name")
            self.case_name = case_name

    def _check_soc_version(self, soc_version):
        utils.check_name_valid(soc_version, name_type="soc version")
        self.soc_version = soc_version

    def _check_device_id(self, device_id):
        if not device_id.isdigit():
            utils.print_error_log(
                'please enter an integer number for device id,'
                ' now is %s.' % device_id)
            raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_DEVICE_ID_ERROR)
        self.device_id = device_id

    def _gen_error_threshold(self, err_thr):
        if err_thr is None:
            err_thr = []
        else:
            try:
                err_thr = ast.literal_eval(err_thr)
            except ValueError as err:
                utils.print_error_log(
                    "Error_threshold is unsupported. Example [0.01, 0.01].")
                raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_ERROR_THRESHOLD_ERROR) from err
            finally:
                pass
        self._check_error_threshold(err_thr)

    def _check_error_threshold(self, err_thr):
        if isinstance(err_thr, list):
            if len(err_thr) == 0:
                self.err_thr = err_thr
                return
            if len(err_thr) == 2:
                self.err_thr = utils.check_list_float(err_thr, "Error_threshold")
                return
        utils.print_error_log(
            "Error_threshold is unsupported. Example [0.01, 0.01].")
        raise utils.OpTestGenException(
            ConstManager.OP_TEST_GEN_INVALID_ERROR_THRESHOLD_ERROR)

    def _check_create_model_quiet(self, ):
        if self.model_path:
            utils.check_path_valid(self.model_path)
            if not self.quiet:
                utils.print_warn_log("Use model function, parameter quiet should be configured.")
            return
        else:
            if self.quiet:
                utils.print_error_log("If you want use quiet function, the parameter model should be configured.")
                raise utils.OpTestGenException(ConstManager.OP_TEST_CREATE_QUIET_ERROR)
            return
