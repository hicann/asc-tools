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
data_generator module
"""
import numpy as np


# pylint: disable=too-many-branches
def gen_data(data_shape, min_value, max_value, dtype, distribution='uniform'):
    """
    generate data
    :param data_shape: the data shape
    :param min_value: min value
    :param max_value: max value
    :param dtype: the data type
    :param distribution: the data distribution
    :return: the numpy data
    """
    supported_distribution = (
        "uniform", "normal", "beta", "laplace", "triangular", "relu", "sigmoid", "softmax", "tanh")
    if distribution not in supported_distribution:
        raise ValueError("distribution should in [%s]" % ",".join(supported_distribution))
    real_dtype = dtype
    if dtype in (bool, "bool",):
        min_value = 0
        max_value = 2  # [0, 2) in uniform
        dtype = np.int8
    if distribution == 'uniform':
        # Returns the uniform distribution random value.
        # min indicates the random minimum value,
        # and max indicates the random maximum value.
        data = np.random.uniform(low=min_value, high=max_value,
                                 size=data_shape).astype(dtype)
    elif distribution == 'normal':
        # Returns the normal (Gaussian) distribution random value.
        # min is the central value of the normal distribution,
        # and max is the standard deviation of the normal distribution.
        # The value must be greater than 0.
        data = np.random.normal(loc=min_value,
                                scale=abs(max_value) + 1e-4,
                                size=data_shape).astype(dtype)
    elif distribution == 'beta':
        # Returns the beta distribution random value.
        # min is alpha and max is beta.
        # The values of both min and max must be greater than 0.
        data = np.random.beta(a=abs(min_value) + 1e-4,
                              b=abs(max_value) + 1e-4,
                              size=data_shape).astype(dtype)
    elif distribution == 'laplace':
        # Returns the Laplacian distribution random value.
        # min is the central value of the Laplacian distribution,
        # and max is the exponential attenuation of the Laplacian
        # distribution.  The value must be greater than 0.
        data = np.random.laplace(loc=min_value,
                                 scale=abs(max_value) + 1e-4,
                                 size=data_shape).astype(dtype)
    elif distribution == 'triangular':
        # Return the triangle distribution random value.
        # min is the minimum value of the triangle distribution,
        # mode is the peak value of the triangle distribution,
        # and max is the maximum value of the triangle distribution.
        mode = np.random.uniform(low=min_value, high=max_value)
        data = np.random.triangular(left=min_value, mode=mode,
                                    right=max_value,
                                    size=data_shape).astype(dtype)
    elif distribution == 'relu':
        # Returns the random value after the uniform distribution
        # and relu activation.
        data_pool = np.random.uniform(low=min_value, high=max_value,
                                      size=data_shape).astype(dtype)
        data = np.maximum(0, data_pool)
    elif distribution == 'sigmoid':
        # Returns the random value after the uniform distribution
        # and sigmoid activation.
        data_pool = np.random.uniform(low=min_value, high=max_value,
                                      size=data_shape).astype(dtype)
        data = 1 / (1 + np.exp(-data_pool))
    elif distribution == 'softmax':
        # Returns the random value after the uniform distribution
        # and softmax activation.
        data_pool = np.random.uniform(low=min_value, high=max_value,
                                      size=data_shape).astype(dtype)
        data = np.exp(data_pool) / np.sum(np.exp(data_pool))
    elif distribution == 'tanh':
        # Returns the random value after the uniform distribution
        # and tanh activation.
        data_pool = np.random.uniform(low=min_value, high=max_value,
                                      size=data_shape).astype(dtype)
        data = (np.exp(data_pool) - np.exp(-data_pool)) / \
               (np.exp(data_pool) + np.exp(-data_pool))
    else:
        pass

    if real_dtype == bool:
        data = data.astype(real_dtype)
    return data
