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
logger
"""
import sys
import inspect
import traceback
import datetime
import time


# 'pylint: disable=too-few-public-methods
class Constant:
    """
    This class for Constant.
    """

    LOG_LEVEL = "INFO"


def set_logger_level(level):
    """
    set logger level
    :param level: level
    :return: None
    """

    Constant.LOG_LEVEL = level


def log(level, file_name, line, msg):
    """
    print log
    :param level: level
    :param file: file_name
    :param line: line
    :param msg: msg
    :return: None
    """

    def _get_time_str():
        time_delta = datetime.timedelta(seconds=-time.timezone)
        tz_obj = datetime.timezone(time_delta)
        return datetime.datetime.now(tz=tz_obj).strftime("%Y-%m-%d %H:%M:%S.%f")

    print("[%s] %s [File \"%s\", line %d] %s" % (level, _get_time_str(), file_name, line, msg))


def log_warn(msg):
    """
    log warn
    :param msg: log msg
    :return: None
    """
    caller = inspect.stack()[1]
    log("WARN", caller.filename, caller.lineno, msg)


def log_debug(msg):
    """
    log debug
    :param msg: log msg
    :return: None
    """
    if Constant.LOG_LEVEL not in ("DEBUG",):
        return
    caller = inspect.stack()[1]
    log("DEBUG", caller.filename, caller.lineno, msg)


def log_info(msg):
    """
    log info
    :param msg: log msg
    :return: None
    """
    if Constant.LOG_LEVEL not in ("DEBUG", "INFO"):
        return
    caller = inspect.stack()[1]
    log("INFO", caller.filename, caller.lineno, msg)


def log_err(msg, print_trace=False):
    """
    log err
    :param msg: log msg
    :return: None
    """
    caller = inspect.stack()[1]
    log("ERROR", caller.filename, caller.lineno, msg)
    if print_trace:
        exc_type, exc_value, exc_traceback = sys.exc_info()
        trace_info = traceback.format_exception(exc_type, exc_value, exc_traceback)
        if trace_info:
            print("".join(trace_info))
