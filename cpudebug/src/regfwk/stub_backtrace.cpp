/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

/* !
 * \file stub_backtrace.cpp
 * \brief
 */
#include <csignal>
#include <sstream>
#include <string>
#include <map>
#include <unistd.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <link.h>
#include <unwind.h>
#include "securec.h"
#include "stub_def.h"

namespace {
std::map<std::string, uint64_t> binaryBaseMap;
}

namespace AscendC {
constexpr int32_t BT_MAX = 24 * 1024;

const std::map<int, std::string> &GetCoreTypeMap()
{
    static const std::map<int, std::string> coreTypeMap = {
        { AscendC::AIC_TYPE, "AIC_" },
        { AscendC::AIV_TYPE, "AIV_" },
        { AscendC::MIX_TYPE, "CORE_" },
    };
    return coreTypeMap;
}

std::map<int, std::string>& GetSignalMessage () {
    static std::map<int, std::string> g_signalMessage = {
        { SIGILL, "SIGILL Signal (Illegal instruction) catched" },
        { SIGBUS, "SIGBUS Signal (Bus error) catched" },
        { SIGFPE, "SIGFPE Signal (Floating point exception) catched" },
        { SIGSEGV, "SIGSEGV Signal (Invalid memory reference) catched" },
        { SIGPIPE, "SIGPIPE Signal (Broken pipe: write to pipe with no readers) catched" },
        { SIGABRT, "SIGABRT Signal (Abort Signal from abort) catched" },
    };
    return g_signalMessage;
}

struct BacktraceData {
    std::string exeObjName;
    std::ostringstream *outStream;
};

// parse the input msg and get the exec file name,
// input msg is like this: output/ascend/host/ubuntu20.04/x86_64/lib/xxxxxx
// the last / means the executed file name
std::string GetExecName(const std::string &msg)
{
    size_t lastSlashPos = msg.find_last_of('/');
    if (lastSlashPos != std::string::npos) {
        return msg.substr(lastSlashPos + 1);
    }

    return msg;
}

// execute addr2line cmd and get the output
std::string ExecCmd(const char *cmd)
{
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buffer[BT_MAX] = "";
        if (fgets(buffer, BT_MAX, fp) == nullptr) {
            return std::string("addr2line excuted failed.");
        }
        (void)pclose(fp);
        return std::string(buffer);
    }
    return std::string("?");
}

std::string GetExecPath();
std::string StackTrace();
bool IsValidBinary(const std::string& exceutePath);

namespace {
// callback function to process each frame
_Unwind_Reason_Code UnwindCallback(struct _Unwind_Context *context, void *arg)
{
    // get pc with unwind function
    int ipBeforeInsn = 0;
    static int32_t frameId = 0;
    uintptr_t pc = _Unwind_GetIPInfo(context, &ipBeforeInsn);
    BacktraceData *btData = reinterpret_cast<BacktraceData *>(arg);
    if (ipBeforeInsn == 0) {
        --pc;
    }

    Dl_info info;
    int32_t ret = dladdr(reinterpret_cast<void *>(pc), &info);
    if (ret == 0) {
        return _URC_NO_REASON;
    }

    std::string objName = GetExecName(info.dli_fname);
    if (binaryBaseMap.count(objName) == 0) {
        return _URC_NO_REASON;
    }

    uint64_t baseAddr = binaryBaseMap[objName];
    uint64_t errorPC = static_cast<uint64_t>(pc) - baseAddr;
    std::vector<char> btCmd(BT_MAX);
    if (!IsValidBinary(info.dli_fname)) {
        return _URC_NO_REASON;
    }

    errno_t err = sprintf_s(btCmd.data(), BT_MAX, "addr2line -e %s -f -p -a -i -C 0x%lx", info.dli_fname, errorPC);
    if (err < 0) {
        return _URC_NO_REASON;
    }
    std::string result = ExecCmd(btCmd.data());
    *(btData->outStream) << "[#" << frameId << "] " << result;
    frameId++;
    return _URC_NO_REASON;
}

// translate cpu debug index to core name in v220
std::string ConvertCpuIdxToCoreName(int idx)
{
    const std::map<int, std::string> &coreTypeMap = GetCoreTypeMap();
    constexpr int subCoreAic = 0;
    constexpr int defaultTaskRation = 2;
    std::string coreName;
    // mix mode
    if (g_kernelMode == KernelMode::MIX_MODE) {
        int idxRes = idx % (defaultTaskRation + 1);
        int blockIdx = idx / (defaultTaskRation + 1);
        int subBlockIdx = 0;
        int flatIdx = 0;
        switch (idxRes) {
            case subCoreAic:
                flatIdx = blockIdx;
                coreName = coreTypeMap.at(AscendC::AIC_TYPE) + std::to_string(flatIdx);
                break;
            default:
                subBlockIdx = idxRes - 1;
                flatIdx = blockIdx * defaultTaskRation + subBlockIdx;
                coreName = coreTypeMap.at(AscendC::AIV_TYPE) + std::to_string(flatIdx);
                break;
        }
        return coreName;
    }

    // unmix mode
    coreName = coreTypeMap.at(AscendC::MIX_TYPE) + std::to_string(idx);
    return coreName;
}

std::string GetMainExecName()
{
    std::string currentObj = GetExecPath();
    std::string exeName = GetExecName(currentObj);
    return exeName;
}

// record all binary name and base addr
int32_t DlCallback(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)data;
    (void)size;
    static int32_t count = 0;
    if (strlen(info->dlpi_name) == 0) {
        // first frame dlpi name is empty, means the main execut obj self
        if (count == 0) {
            binaryBaseMap.insert(std::make_pair(GetMainExecName(), info->dlpi_addr));
        }
    } else {
        binaryBaseMap.insert(std::make_pair(GetExecName(info->dlpi_name), info->dlpi_addr));
    }
    count++;
    return 0;
}
} // namespace
// reduce the print content and avoid some mistakes
bool IsValidBinary(const std::string& exceutePath)
{
    std::vector<std::string> skipPath = {"/lib", "/usr/lib", "/usr/local/lib"};
    for (const std::string& path : skipPath) {
        size_t n = exceutePath.find(path);
        // remove system files debug info
        if (n == 0) {
            return false;
        }
    }

    std::vector<std::string> skipFile = {"linux-vdso", "libtikicpulib_stubreg.so"};
    std::string objName = GetExecName(exceutePath);
    for (const std::string& file : skipFile) {
        size_t n = objName.find(file);
        // remove system files debug info
        if (n == 0) {
            return false;
        }
    }

    return true;
}

// generate error message
std::string GetExecPath()
{
    char result[BT_MAX];
    ssize_t count = readlink("/proc/self/exe", result, BT_MAX);
    return std::string(result, (count > 0) ? count : 0);
}

std::string StackTrace()
{
    dl_iterate_phdr(DlCallback, nullptr);
    std::ostringstream stacktraceStream;
    std::string exeName = GetMainExecName();
    BacktraceData btData = { exeName, &stacktraceStream };
    _Unwind_Backtrace(UnwindCallback, &btData);
    // return the backtrace info str
    std::string stackTrace = stacktraceStream.str();
    return stackTrace;
}

// generate core name from cpu debug index for all soc version
std::string GetCoreName(int idx)
{
    int coreIdx = idx;
    std::string coreName = "CORE_" + std::to_string(coreIdx);
    if (g_socVersion == SocVersion::VER_220 || g_socVersion == SocVersion::VER_310 ||\
        g_socVersion == SocVersion::VER_510) {
        coreName = ConvertCpuIdxToCoreName(idx);
    }
    return coreName;
}

void BacktracePrint(int sig)
{
    int flatIdx = block_idx;
    const std::map<int, std::string> &coreTypeMap = GetCoreTypeMap();
    std::string coreName = coreTypeMap.at(AscendC::MIX_TYPE);
    if (g_socVersion == SocVersion::VER_220 || g_socVersion == SocVersion::VER_310 ||\
        g_socVersion == SocVersion::VER_510) {
        if (g_kernelMode == KernelMode::MIX_MODE) {
            coreName = coreTypeMap.at(g_coreType);
            flatIdx = (g_coreType == AscendC::AIC_TYPE) ? block_idx : (block_idx * g_taskRation + sub_block_idx);
        }
    }
    coreName += std::to_string(flatIdx);
    // samples: [ERROR][AIV_0][pid 12] error happened! =========
    std::string ret = "[ERROR][" + coreName + "][pid ";
    ret += std::to_string(getpid());
    ret += "] error happened! ========= \n";
    // samples: SIGSEGV Signal (Invalid memory reference) catched, backtrace info:
    ret += GetSignalMessage()[sig] + ", backtrace info:\n";
    ret += StackTrace();
    // using process lock to keep content clear
    AscendC::KernelPrintLock::GetLock()->Lock();
    std::cout << ret << std::endl;
    AscendC::KernelPrintLock::GetLock()->Unlock();
}
} // namespace AscendC