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
 * \file kern_fwk.h
 * \brief
 */
#ifndef ASCENDC_ICPU_FWK_H
#define ASCENDC_ICPU_FWK_H
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include <sys/wait.h>
#include "stub_def.h"
#include "stub_reg.h"
#include "securec.h"
#include "kernel_common.h"

inline void CheckSysWorspaceSet(uint8_t* ptr)
{
    if (ptr == nullptr) {
        printf("[error]g_sysWorkspaceReserved is null, g_sysWorkspaceReserved has been set or not\n");
    }
}

inline void AscendCExit()
{
    exit(1);
}

inline void Handler(int sig)
{
    if (sig != SIGINT) {
        AscendC::BacktracePrint(sig);
    }
    AscendC::KernelPrintLock::FreeLock();
    AscendC::ProcessLock::FreeLock();
    if (getpid() == g_mainPid) {
        for (int32_t idx = 0; idx < g_processNum; idx++) {
            int status = 0;
            waitpid(GetProcessId()[idx], &status, 0);
            std::cout << "[pid " + std::to_string(GetProcessId()[idx]) + \
                "] exit status:" + std::to_string(status) << std::endl;
        }
        for (auto& tmpFile : GetTmpFileName()) {
            struct stat buffer;
            if (stat(tmpFile.c_str(), &buffer) == 0) {
                remove(tmpFile.c_str());
            }
        }
    }
    AscendCExit();
}

#define ICPU_SET_TILING_KEY(tilingKey) \
    do {                               \
        g_tilingKey = tilingKey;       \
    } while (0)

inline void AscendCNpuCheckEnInterruptExit(void)
{
#ifdef ASCENDC_NPUCHK_INTER_EXIT
    AscendCEnInterruptExit();
#endif
}

namespace AscendC{
template<typename T, typename... Args>
void RunKernelFunctionOnCpu(T kernelFunc, const char* funcName, unsigned numBlocks, Args... args)
{
    g_mainPid = getpid();
    AscendC::CheckNumBlocksForFfts(numBlocks);
    AscendC::InitSocVersion();
    constexpr size_t workspaceSize = AscendC::RESERVED_WORKSPACE;
    uint8_t* sysWorkSpacePtr = (uint8_t*)AscendC::GmAlloc(workspaceSize);
    memset_s(sysWorkSpacePtr, workspaceSize, 0, workspaceSize);
    constexpr size_t fftsCounterSize = AscendC::GetMaxCoreNum() * AscendC::MIX_IN_GROUP_CORE_NUM *
        AscendC::FLAG_NUM * AscendC::FFTS_COUNTER_NUM;
    void* ffts_addr = AscendC::GmAlloc(fftsCounterSize);
    memset_s(ffts_addr, fftsCounterSize, 0, fftsCounterSize);
    CheckSysWorspaceSet(sysWorkSpacePtr);
    g_sysWorkspaceReserved = sysWorkSpacePtr;
    size_t argn = sizeof...(args);
    uint64_t kargs[argn];

    {
        size_t i = 0;
        (void)std::initializer_list<int>{
            ((kargs[i++] = (uint64_t)(*(uint64_t*)(&args))), 0)...
        };  
    }
    AscendC::StubInit();
    #ifndef ASCENDC_NPUCHK_OFF
    AscendCKernelBegin(funcName, argn, kargs);
    AscendCNpuCheckEnInterruptExit();
    #endif
    block_num = numBlocks;
    int processNum = get_process_num();
    g_processNum = processNum;
    int blks[processNum];
    int idx = 0;
    std::unordered_map<int, std::string> procNameMap;
    AscendC::ConstDefiner::Instance();
    AscendC::KernelPrintLock::GetLock();
    AscendC::ProcessLock::GetProcessLock();
    fflush(stdout);
    for (idx = 0; idx < processNum; ++idx) {
        set_block_dim(idx);
        int pid = fork();
        blks[idx] = pid;
        GetProcessId().push_back(pid);
        procNameMap.insert({ pid, AscendC::GetCoreName(idx) });
        if (pid == 0) {
            struct sigaction act;
            act.sa_handler = Handler;
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
            sigaction(SIGILL, &act, 0);
            sigaction(SIGBUS, &act, 0);
            sigaction(SIGFPE, &act, 0);
            sigaction(SIGSEGV, &act, 0);
            sigaction(SIGPIPE, &act, 0);
            sigaction(SIGABRT, &act, 0);
            sigaction(SIGINT, &act, 0);
            set_core_type(idx);
            #ifndef ASCENDC_NPUCHK_OFF
            AscendCBlockBegin(static_cast<int32_t>(block_idx), funcName, argn, kargs);
            #endif
            AscendC::CheckGmValied(argn, kargs);
            set_ffts_base_addr(reinterpret_cast<uint64_t>(ffts_addr));
            #ifndef ASCENDC_NPUCHK_OFF
            try {
                kernelFunc(args...);
                AscendC::CheckSyncState();
            } catch (std::logic_error& err) {
                std::cout << "[NPUCHECK ERROR]: " << err.what() <<  std::endl;
                AscendCBlockEnd(static_cast<int32_t>(block_idx), funcName, argn, kargs);
                exit(-1);
            }
            AscendCBlockEnd(static_cast<int32_t>(block_idx), funcName, argn, kargs);
            #else
            kernelFunc(args...);
            AscendC::CheckSyncState();
            #endif
            exit(0);
        }
    }
    struct sigaction act;
    act.sa_handler = Handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGFPE, &act, 0);
    sigaction(SIGILL, &act, 0);
    sigaction(SIGBUS, &act, 0);
    sigaction(SIGSEGV, &act, 0);
    sigaction(SIGPIPE, &act, 0);
    sigaction(SIGABRT, &act, 0);
    sigaction(SIGINT, &act, 0);
    if (idx >= processNum) {
        for (idx = 0; idx < processNum; ++idx) {
            int status;
            waitpid(blks[idx], &status, 0);
            if (status == 0) {
                std::cout << "[SUCCESS][" << procNameMap[blks[idx]] <<
                    "][pid " + std::to_string(blks[idx]) + "] exit success!" << std::endl;
            }
        }
        #ifndef ASCENDC_NPUCHK_OFF
        AscendCKernelEnd(funcName, argn, kargs);
        #endif
    }
    AscendC::GmFree((void*)sysWorkSpacePtr);
    g_sysWorkspaceReserved = nullptr;
    AscendC::GmFree(ffts_addr);
    #ifndef ASCENDC_NPUCHK_OFF
    AscendC::KernelPrintLock::FreeLock();
    AscendC::ProcessLock::FreeLock();
    #endif
}
}; // namespace AscendC

#define ICPU_RUN_KF(func, numBlocks, ...)                                                                   \
    do {                                                                                                 \
        AscendC::RunKernelFunctionOnCpu(func, #func, numBlocks, ##__VA_ARGS__);                                      \
    } while (0)

#endif // ASCENDC_ICPU_FWK_H
