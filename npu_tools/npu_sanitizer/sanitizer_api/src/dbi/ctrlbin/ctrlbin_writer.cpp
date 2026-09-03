// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "ctrlbin_bindings.h"

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unordered_map>

#include "ctrlbin_serialize.h"
#include "ctrlbin_file_io.h"
#include "ctrlbin_singleton.h"
#include "ctrlbin_string_utils.h"

using namespace std;

namespace {
// ===================== 以下结构体是和bisheng-tune组件约定的数据结构，请勿改动 =========
constexpr uint8_t FUNC_NAME_SEP = 12;
const string FUNC_SEP = Serialize(FUNC_NAME_SEP);
#pragma pack(4)
struct FuncNameList {
    uint32_t size;     // 这个结构体的大小
    char funcLists[0]; // 存放字符串数组以0为间隔，默认以编号0开始
};

// 整个文件的结构
struct CtrlInfo {
    uint32_t size;         // 整个结构体大小
    uint16_t version;      // 兼容性考虑
    uint16_t splitOffset1; // 切分点
    uint16_t reserve[2];
    uint8_t
        data[0]; // 这个data里面第一部分是funcInfoSize大小的FuncInfos，第二部分是FuncNameList，从splitOffset1开始到结束
};

// 单个插桩函数的信息
struct FuncInfo {
    uint16_t instrId;      // 插入指令的ID编码，现在先是gm_to_ubuf
    uint16_t injectFuncId; // 从FuncNameList中获得function，依据编号，可以找到function name
    uint16_t paraNum;      // 参数的个数，如：5
    uint16_t paraMask[0];  // 具体参数的掩码，如：1|0   2    0    1   1|1 这样的
};
#pragma pack()

// ====================== 以上结构体是和bisheng-tune组件约定的数据结构，请勿改动

struct CtrlbinBinding {
    uint16_t instrId{};
    uint16_t injectFuncId{};
    vector<uint16_t> paraMask;

    void Serialize(stringstream& ss) const
    {
        FuncInfo info{instrId, injectFuncId, static_cast<uint16_t>(paraMask.size())};
        ss << string(reinterpret_cast<const char*>(&info), sizeof(FuncInfo));
        ss << string(reinterpret_cast<const char*>(paraMask.data()), sizeof(uint16_t) * info.paraNum);
    }
};

class CtrlbinBindingRegistry : public Singleton<CtrlbinBindingRegistry, true> {
public:
    friend class Singleton<CtrlbinBindingRegistry, true>;

    void Reset()
    {
        funcNames_.clear();
        injectFuncIdMap_.clear();
        injectInfo_.clear();
    }

    void RegisterCtrlbinBinding(InstrType instrType, const string& injectedFuncName, const vector<uint16_t>& paraMask)
    {
        if (injectFuncIdMap_.count(injectedFuncName) == 0) {
            injectFuncIdMap_[injectedFuncName] = static_cast<uint16_t>(funcNames_.size());
            funcNames_.push_back(injectedFuncName);
        }

        CtrlbinBinding injectInfo;
        injectInfo.instrId = static_cast<uint16_t>(instrType);
        injectInfo.injectFuncId = injectFuncIdMap_[injectedFuncName];
        injectInfo.paraMask = paraMask;
        injectInfo_.push_back(injectInfo);
    }

    void Serialize(stringstream& ss)
    {
        stringstream buffer;
        for (const auto& info : injectInfo_) {
            info.Serialize(buffer);
        }

        CtrlInfo ctrlInfo{};
        ctrlInfo.splitOffset1 = buffer.str().size();
        string funcLists = Join(funcNames_.begin(), funcNames_.end(), FUNC_SEP);

        // series funcLists
        FuncNameList funcNameList{static_cast<uint32_t>(funcLists.size() + sizeof(FuncNameList))};
        buffer << string(reinterpret_cast<const char*>(&funcNameList), sizeof(FuncNameList));
        buffer << funcLists;
        // Preserve the established ctrl.bin layout without reading beyond funcLists.data().
        buffer << string(sizeof(FuncNameList), '\0');
        string bufferStr = buffer.str();
        ctrlInfo.size = bufferStr.size() + sizeof(CtrlInfo);
        ctrlInfo.version = 0;
        ctrlInfo.reserve[0] = 0;
        ctrlInfo.reserve[1] = 0;

        ss << string(reinterpret_cast<const char*>(&ctrlInfo), sizeof(CtrlInfo));
        ss << bufferStr;
    }

private:
    CtrlbinBindingRegistry() = default;

private:
    vector<string> funcNames_;
    unordered_map<string, uint16_t> injectFuncIdMap_;
    vector<CtrlbinBinding> injectInfo_;
};

void WriteToBin(const string& outputData, const string& filepath)
{
    std::string realPath = filepath;
    if (!CheckWriteFilePathValid(realPath)) {
        return;
    }
    ofstream writer(realPath, ios::binary | ios::out);
    writer.write(outputData.c_str(), static_cast<std::streamsize>(outputData.size()));
    writer.close();
    Chmod(realPath, SAVE_DATA_FILE_AUTHORITY);
}

// 生成的数据格式是和bisheng-tune组件约定好的结构
void WriteCtrlbin(const string& outputPath)
{
    stringstream ss;
    CtrlbinBindingRegistry::Instance().Serialize(ss);
    WriteToBin(ss.str(), outputPath);
}
} // namespace

void RegisterCtrlbinBinding(InstrType instrType, const string& injectedFuncName, const vector<uint16_t>& paraMask)
{
    CtrlbinBindingRegistry::Instance().RegisterCtrlbinBinding(instrType, injectedFuncName, paraMask);
}

extern "C" {
void RegisterCtrlbinBindings() { return; }

void CtrlbinWriterStart(const char* output, uint16_t length)
{
    CtrlbinBindingRegistry::Instance().Reset();
    RegisterCtrlbinBindings();
    string outputPath(output, length);
    WriteCtrlbin(outputPath);
}
}
