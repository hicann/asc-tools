/**
 * @file main.cpp
 *
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */
#include "op_test.h"
#include "acl/acl.h"
#include "common.h"

int main(int argc, char *argv[])
{
    int ret =  OpTest::UnitTest::GetInstance().Run();
    if (aclrtDestroyStream(g_stream) != ACL_SUCCESS) {
        ERROR_LOG("aclrtDestroyStream failed.");
    }
    if (aclFinalize() != ACL_SUCCESS) {
        ERROR_LOG("aclFinalize failed.");
    }
    return ret;
}
