#!/usr/bin/env python
# -*- coding:utf-8 -*-
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
file util module
"""
import os
import stat


# 'pylint: disable=too-few-public-methods
class Constant:
    """
    This class for Constant.
    """
    DATA_DIR_MODES = stat.S_IWUSR | stat.S_IRUSR | stat.S_IXUSR | stat.S_IRGRP | stat.S_IXGRP


# 'pylint: disable=unused-argument,invalid-name,unused-variable
def _mkdir_without_file_exist_err(dir_path, mode):
    try:
        os.mkdir(dir_path, mode)
    except FileExistsError as err:
        pass


def makedirs(path, mode=Constant.DATA_DIR_MODES):
    """
    like sheel makedir
    :param path: dirs path
    :param mode: dir mode
    :return: None
    """

    def _rec_makedir(dir_path):
        parent_dir = os.path.dirname(dir_path)
        if parent_dir == dir_path:
            # root dir, not need make
            return
        if not os.path.exists(parent_dir):
            _rec_makedir(parent_dir)
            _mkdir_without_file_exist_err(dir_path, mode)
        else:
            _mkdir_without_file_exist_err(dir_path, mode)

    path = os.path.realpath(path)
    _rec_makedir(path)


def check_file_valid(file_path, size_limit=134217728):
    if not isinstance(size_limit, int):
        raise TypeError("size_limit needs to be an integer, not %s" % str(type(size_limit)))
    file_path = os.path.realpath(file_path)
    if not os.path.exists(file_path):
        raise IOError("file_path is not exist.")
    file_size = os.stat(file_path, follow_symlinks=True).st_size
    if file_size > size_limit:
        raise IOError("File is too large! Size of % exceeds the limit: %d") % (file_path, size_limit)


def read_file(file_path: str, size_limit: int = 134217728) -> bytes:
    """
    read file

    Parameters
    ----------
    file_path : str
        Path to the file
    size_limit : int, option
        Raise an Exception if the file is too large

    Returns
    -------
    data_content : bytes
        The data content
    """
    check_file_valid(file_path, size_limit)
    with open(file_path, "rb") as ff:
        file_content = ff.read(-1)
    return file_content
