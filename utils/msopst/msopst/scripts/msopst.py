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
import os
import sys

script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(os.path.dirname(script_dir))
if project_root not in sys.path:
    sys.path.insert(0, project_root)

from atexit import register
from msopst.st.interface import utils
from msopst.st.interface.arg_parser import MsopstArgParser
from msopst.st.interface.acl_op_runner import AclOpRunner
from msopst.st.interface.ms_op_runner import MsOpRunner
from msopst.st.interface.acl_op_generator import AclOpGenerator
from msopst.st.interface.gen_ascendc_test import GenAscendCOpTestCode
from msopst.st.interface.ms_op_generator import MsOpGenerator
from msopst.st.interface.pt_op_generator import PtOpGenerator
from msopst.st.interface.case_design import CaseDesign
from msopst.st.interface.case_generator import CaseGenerator
from msopst.st.interface.data_generator import DataGenerator
from msopst.st.interface import st_report
from msopst.st.interface.advance_ini_parser import AdvanceIniParser
from msopst.st.interface.const_manager import ConstManager
from msopst.st.interface.atc_transform_om import AtcTransformOm
from msopst.st.interface.result_comparer import ResultCompare
abnormal_termination = False


def _create_op_generator(case_list, output_path, args, machine_type, report, advance_args=None):
    if case_list[0].get(ConstManager.ST_MODE) == 'ms_python_train':
        return MsOpGenerator(case_list, (output_path, args.device_id), machine_type, report)
    if case_list[0].get(ConstManager.ST_MODE) == 'pt_python_train':
        return PtOpGenerator(
            case_list, (output_path, args.device_id), machine_type, (report, advance_args))


def _generate_run_cmd_op_project(args, report, machine_type, get_advance_args=None):
    # design test case list from json file
    design = CaseDesign(args.input_file, args.case_name, report)
    case_list, compile_flag = design.design()
    output_path = os.path.realpath(args.output_path)
    if not case_list:
        utils.print_error_log("There is no testcase to generate. Please modify the testcase json.")
        raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_PARAM_ERROR)
    if 'op' not in case_list[0]:
        utils.print_error_log("The key 'op' is missing.")
        raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_PARAM_ERROR)
    # generate acl project or python test scripts and generate test data
    if utils.is_gen_python_st(case_list[0]):
        op_generator_instance = _create_op_generator(
            case_list, output_path, args, machine_type, report, get_advance_args)
        op_generator_instance.generate()
    else:
        # atc tools transform acl_op.json of single operator to om model.
        atc_transform = AtcTransformOm(case_list, output_path, compile_flag, machine_type, report)
        atc_transform.transform_acl_json_to_om(args.soc_version, get_advance_args)
        acl_op_generator_instance = AclOpGenerator(case_list, (output_path, args.device_id, machine_type), report)
        acl_op_generator_instance.generate()
        path = os.path.join(output_path,
                            case_list[0]['op'].replace('/', '_'))
        runner = AclOpRunner(path, args.soc_version, report, get_advance_args)
        runner.acl_compile()
    # generate data
    data_generator = DataGenerator(case_list, output_path, False, report)
    data_generator.generate()
    return case_list


def _runner_and_compare(args, case_list, report, get_advance_args=None):
    # run operator
    output_op_path = os.path.join(args.output_path,
                                  case_list[0]['op'].replace('/', '_'))
    path = os.path.realpath(output_op_path)
    if case_list[0].get('st_mode') == 'ms_python_train':
        op_name_fixed = utils.fix_name_lower_with_under(case_list[0]['op'])
        runner = MsOpRunner(path, op_name_fixed, args.soc_version, report)
        runner.process()
    else:
        runner = AclOpRunner(path, args.soc_version, report, get_advance_args)
        if case_list[0].get('st_mode') == 'pt_python_train':
            runner.run_torch_api(case_list[0]['op'])
        else:
            runner.run()
            # compare
            compare_obj = ResultCompare(report, path, args.err_thr, args.error_report)
            compare_obj.compare()
    report.console_print()


def _generate_testcase(args, report, machine_type):
    # design test case list from st_report.json file
    report_path = args.input_file
    utils.check_path_valid(report_path)
    report.load(report_path)
    case_list = []
    for idx, report_obj in enumerate(report.report_list):
        case_list.append(report_obj.trace_detail.st_case_info.op_params)
    output_path = os.path.realpath(args.output_path)
    if len(case_list) == 0:
        utils.print_error_log("The st_report.json doesn't include operator information.")
        raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_PARAM_ERROR)
    if utils.is_gen_python_st(case_list[0]):
        op_generator_instance = _create_op_generator(
            case_list, output_path, args, machine_type, report)
        op_generator_instance.generate()
    else:
        # atc tools transform acl_op.json of single operator to om model.
        acl_op_generator_instance = AclOpGenerator(case_list, (output_path, args.device_id, machine_type), report)
        acl_op_generator_instance.generate()
    # generate data
    data_generator = DataGenerator(case_list, output_path, machine_type, report)
    data_generator.generate()


def _gen_ascendc_test_cmd(args, report, machine_type):
    # design test case list from json file
    design = CaseDesign(args.input_file, 'all', report)
    case_list, _ = design.design()
    output_path = os.path.realpath(args.output_path)
    if not case_list:
        utils.print_error_log("There is no testcase to generate. Please modify the testcase json.")
        raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_PARAM_ERROR)
    if len(case_list) > 1:
        utils.print_error_log("There is only supporting one testcase for AscendC operators, please check it.")
        raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_PARAM_ERROR)
    # generate data for AscendC operator testcase.
    data_generator = DataGenerator(case_list, output_path, machine_type, report)
    data_generator.gen_data_for_ascendc(output_path)
    # generate test code from json file and kernel file.
    kernel_file = os.path.realpath(args.kernel_file)
    gen_ascendc_test = GenAscendCOpTestCode(case_list, kernel_file, output_path, machine_type)
    gen_ascendc_test.generate()


def _do_run_cmd(args, report):
    try:
        # set expect value from json
        input_json_path = os.path.realpath(args.input_file)
        report.set_expect(input_json_path)
        if args.config_file != "":
            _gen_build_convert_run(args, report)
        else:
            case_list = _generate_run_cmd_op_project(args, report, False)
            _runner_and_compare(args, case_list, report)

    except utils.OpTestGenException as ex:
        report.console_print()
        sys.exit(ex.error_info)
    finally:
        pass


def _gen_build_convert_run(args, report):
    output_path = os.path.realpath(args.output_path)
    get_advance_args = AdvanceIniParser(args.config_file)
    get_advance_args.get_advance_args_option()
    acl_mode = get_advance_args.get_mode_flag()
    if acl_mode == ConstManager.BOTH_GEN_AND_RUN_ACL_PROJ or \
            acl_mode == ConstManager.BOTH_GEN_AND_RUN_ACL_PROJ_PERFORMANCE:
        # gen op project and build & convert & execute the project.
        case_list = _generate_run_cmd_op_project(args, report, False, get_advance_args)
        _runner_and_compare(args, case_list, report, get_advance_args)
    elif acl_mode == ConstManager.ONLY_GEN_WITHOUT_RUN_ACL_PROJ:
        # only gen op project.
        _generate_run_cmd_op_project(args, report, False, get_advance_args)
        report.console_print()
    elif acl_mode == ConstManager.ONLY_RUN_WITHOUT_GEN_ACL_PROJ or \
            acl_mode == ConstManager.ONLY_RUN_WITHOUT_GEN_ACL_PROJ_PERFORMANCE:
        # only build & convert & execute the project.
        # when the report is input,should avoid update the report
        global abnormal_termination
        abnormal_termination = True
        output_path = os.path.dirname(output_path)
        report_path = os.path.join(output_path, 'st_report.json')
        utils.check_path_valid(report_path)
        report.load(report_path)
        utils.print_info_log("Load %s success." % report_path)
        op_case_list = utils.load_json_file(args.input_file)
        if len(op_case_list) == 0:
            utils.print_error_log("Failed to get %s, please check."
                                  % args.input_file)
            raise utils.OpTestGenException(
                utils.OP_TEST_GEN_INVALID_PATH_ERROR)
        op_name = op_case_list[0].get('op')
        st_mode = op_case_list[0].get('st_mode')
        # run operator
        path = os.path.join(output_path, op_name)
        if st_mode == 'ms_python_train':
            op_name_fixed = utils.fix_name_lower_with_under(op_name)
            runner = MsOpRunner(path, op_name_fixed, args.soc_version, report)
            runner.process()
        else:
            runner = AclOpRunner(path, args.soc_version, report, get_advance_args)
            if st_mode == 'pt_python_train':
                runner.run_torch_api(op_name)
            else:
                runner.run()
                # compare
                compare_obj = ResultCompare(report, path, args.err_thr, args.error_report)
                compare_obj.compare()
        report.console_print()
        report.save(report_path)
        utils.print_info_log('The st report saved in: %s.' % report_path)


def save_report(report, output_path):
    if abnormal_termination:
        return
    report_file = os.path.join(os.path.realpath(output_path),
                               'st_report.json')
    try:
        report.save(report_file)
        utils.print_info_log('The st report saved in: %s.' % report_file)
    except (OSError, utils.OpTestGenException) as ex:
        sys.exit(ex.error_info)


def main():
    """
    main function
    :return:
    """
    # parse input argument
    try:
        args = MsopstArgParser()
    except utils.OpTestGenException as ex:
        sys.exit(ex.error_info)
    finally:
        pass
    # run/mi gen command generator report.
    report = st_report.OpSTReport(" ".join(sys.argv))
    register(save_report, report=report, output_path=args.output_path)
    if sys.argv[1] == 'create':
        # generate test_case.json
        try:
            global abnormal_termination
            abnormal_termination = True
            generator = CaseGenerator(args)
            generator.generate()
        except utils.OpTestGenException as ex:
            sys.exit(ex.error_info)
        finally:
            pass
    elif sys.argv[1] == 'ascendc_test':
        try:
            _gen_ascendc_test_cmd(args, report, False)
        except utils.OpTestGenException as ex:
            sys.exit(ex.error_info)
    elif sys.argv[1] == 'run':
        _do_run_cmd(args, report)
    utils.print_info_log("Process finished!")
    sys.exit(ConstManager.OP_TEST_GEN_NONE_ERROR)


if __name__ == "__main__":
    main()
