#!/usr/bin/env python
# coding=utf-8
"""
Function:
AclOpGenerator class. This class mainly implements acl op src code generation.
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

Change History: 2020-07-11 file Created
"""

import os
import importlib

from msopst.st.interface import utils
from msopst.st.interface.const_manager import ConstManager


def _get_framework_type(path):
    cur_dir = os.path.split(os.path.realpath(__file__))[0]
    config_path = os.path.join(cur_dir, ConstManager.FRAMEWORK_CONFIG_PATH)
    framework_dict = utils.load_json_file(config_path)
    suffix_list = []
    for (key, value) in list(framework_dict.items()):
        for item in value:
            suffix_list.append(item)
            if path.endswith(item):
                return key
    utils.print_error_log(
        'The model file "%s" is invalid, only supports %s file. '
        'Please modify it.' % (path, suffix_list))
    raise utils.OpTestGenException(
        ConstManager.OP_TEST_GEN_INVALID_PARAM_ERROR)


def _function_call(args, op_type, func_name):
    framework = _get_framework_type(args.model_path)
    module_name = 'msopst.st.interface.framework.%s_model_parser' % \
                  framework
    utils.print_info_log("Start to import %s." % module_name)
    module = importlib.import_module(module_name)
    func = getattr(module, func_name)
    try:
        return func(args, op_type)
    except Exception as ex:
        utils.print_error_log(
            'Failed to execute "%s". %s' % (func_name, str(ex)))
        raise utils.OpTestGenException(ConstManager.OP_TEST_GEN_INVALID_PARAM_ERROR) from ex
    finally:
        pass


def get_model_nodes(args, op_type):
    """
    get model nodes by framework
    :param op_type: the op type
    :param args: the argument
    :return: the list of nodes.
    eg:
    node_list:
    [{"op_type": 'Add',
    "layer": 'fp32_vars/add',
    "input_dtype": ['float','float'],
    "input_shape": [[8,56,56,256],[8,56,56,256]],
    "output_dtype": ['float'],
    "output_shape": [[8,56,56,256]],
    "attr": [{'name :'T', type:'type', value:'AT_FLOAT'}]
    }]
    """
    return _function_call(args, op_type, ConstManager.GET_MODEL_NODES_FUNC)
