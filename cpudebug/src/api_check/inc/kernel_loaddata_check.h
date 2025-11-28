/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

/*!
 * \file kernel_loaddata_check.h
 * \brief
 */

#ifndef ASCENDC_LOAD_DATA_CHECK_H
#define ASCENDC_LOAD_DATA_CHECK_H
#include "kernel_base_check.h"

namespace AscendC {
namespace check {
class TikcppLoaddata2dCheck : public TikcppBaseCheck {
public:
    TikcppLoaddata2dCheck(const std::string& name, LoadData2dApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppLoaddata2dCheck() override = default;
    bool CheckAllHighLevel();
public:
    LoadData2dApiParams& param_;
};

class TikcppLoaddata2dv2Check : public TikcppBaseCheck {
public:
    TikcppLoaddata2dv2Check(const std::string& name, LoadData2dv2ApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppLoaddata2dv2Check() override = default;
    bool CheckAllHighLevel() const;
public:
    LoadData2dv2ApiParams& param_;
};

class TikcppLoaddata3dv1Check : public TikcppBaseCheck {
public:
    TikcppLoaddata3dv1Check(const std::string& name, LoadData3dv1ApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppLoaddata3dv1Check() override = default;
    bool CheckAllHighLevel() const;
public:
    LoadData3dv1ApiParams& param_;
};

class TikcppLoaddata3dv2Check : public TikcppBaseCheck {
public:
    TikcppLoaddata3dv2Check(const std::string& name, LoadData3dv2ApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppLoaddata3dv2Check() override = default;
    bool CheckAllHighLevel();
public:
    LoadData3dv2ApiParams& param_;
};

class TikcppLoaddata3dv2ProCheck : public TikcppBaseCheck {
public:
    TikcppLoaddata3dv2ProCheck(const std::string& name, LoadData3dv2ProApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppLoaddata3dv2ProCheck() override = default;
    bool CheckAllHighLevel();
public:
    LoadData3dv2ProApiParams& param_;
};

class TikcppLoadImageToLocalCheck : public TikcppBaseCheck {
public:
    TikcppLoadImageToLocalCheck(const std::string& name, LoadImageToLocalApiParams& param)
        : TikcppBaseCheck(name), param_(param) {}
    ~TikcppLoadImageToLocalCheck() override = default;
    bool CheckAllHighLevel();
public:
    LoadImageToLocalApiParams& param_;
};
}
}
#endif