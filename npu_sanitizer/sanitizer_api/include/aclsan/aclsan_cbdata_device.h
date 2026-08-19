/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_CBDATA_DEVICE_H
#define ACLSAN_CBDATA_DEVICE_H

#include "aclsan/aclsan_cbdata_common.h"

#define ACLSAN_RAW_ARG_MAX 6

// device对应的物理位置
typedef enum AclsanDeviceMemorySpace {
    ACLSAN_DEVICE_MEMORY_SPACE_UNKNOWN = 0,
    ACLSAN_DEVICE_MEMORY_SPACE_GM = 1,
    ACLSAN_DEVICE_MEMORY_SPACE_UB = 2,
    ACLSAN_DEVICE_MEMORY_SPACE_L1 = 3,
    ACLSAN_DEVICE_MEMORY_SPACE_L0A = 4,
    ACLSAN_DEVICE_MEMORY_SPACE_L0B = 5,
    ACLSAN_DEVICE_MEMORY_SPACE_L0C = 6,
    ACLSAN_DEVICE_MEMORY_SPACE_BT = 7,
    ACLSAN_DEVICE_MEMORY_SPACE_PRIVATE = 8 // TODO: 确认是否有用
} AclsanDeviceMemorySpace;

typedef enum AclsanDeviceMemoryAccessMode {
    ACLSAN_DEVICE_MEMORY_ACCESS_READ = 1,
    ACLSAN_DEVICE_MEMORY_ACCESS_WRITE = 2,
    ACLSAN_DEVICE_MEMORY_ACCESS_READ_WRITE = 3
} AclsanDeviceMemoryAccessMode;

typedef struct AclsanDeviceEventHeader {
    uint32_t version;
    uint32_t size;
    uint64_t launchId;
    uint64_t pc;
    uint32_t siteId;
    uint32_t sourceKind;
    uint64_t instrExecId;
    uint64_t serialNo;
    uint32_t deviceId;
    uint32_t coreId;
    uint32_t blockId;
    uint32_t blockType;
    uint32_t pipeline;
    uint32_t flags;
} AclsanDeviceEventHeader;

// TODO: 待确认这个结构体的作用是什么
typedef struct AclsanDeviceInstructionData {
    AclsanCallbackCommonData common;
    uint32_t pipeline;
    uint32_t cbid;
    uint32_t siteId;
    uint32_t blockId;
    uint64_t launchId;
    uint64_t binaryId;
    uint64_t functionId;
    uint64_t pc;
    uint64_t rawArgs[ACLSAN_RAW_ARG_MAX];
} AclsanDeviceInstructionData;

// ==========================================
// ===========      MEM CHECK      ==========
// ==========================================
// 内存访问模板: 单点访问 / 连续范围 / stride跳跃 / 多维
typedef enum AclsanMemLayoutKind {
    ACLSAN_MEM_LAYOUT_SCALAR = 1,
    ACLSAN_MEM_LAYOUT_RANGE = 2,
    ACLSAN_MEM_LAYOUT_BLOCK_REPEAT = 3,
    ACLSAN_MEM_LAYOUT_ND_AFFINE = 4
} AclsanMemLayoutKind;

typedef struct AclsanMemScalarLayout {
    uint32_t bytes;
    uint32_t reserved;
} AclsanMemScalarLayout;

typedef struct AclsanMemRangeLayout {
    uint64_t bytes;
} AclsanMemRangeLayout;

typedef struct AclsanMemBlockRepeatLayout {
    uint32_t blockNum;
    uint32_t blockSize;
    int64_t blockStride;
    uint32_t repeatTimes;
    uint32_t reserved;
    int64_t repeatStride;
} AclsanMemBlockRepeatLayout;

typedef struct AclsanMemNdAffineLayout {
    uint32_t rank;
    uint32_t elementBytes;
    uint64_t dims[5];
    int64_t strides[5];
} AclsanMemNdAffineLayout;

typedef struct AclsanDeviceMemoryAccessData {
    AclsanDeviceEventHeader header;
    uint64_t address;
    uint32_t memorySpace;
    uint32_t accessMode;
    uint32_t accessIndex;
    uint32_t accessCount;
    uint32_t dataBits;
    uint32_t alignSize;
    uint64_t vectorMask0;
    uint64_t vectorMask1;
    uint64_t predicateMask0;
    uint64_t predicateMask1;
    uint32_t layoutKind;
    uint32_t memoryFlags;
    union {
        AclsanMemScalarLayout scalar;
        AclsanMemRangeLayout range;
        AclsanMemBlockRepeatLayout blockRepeat;
        AclsanMemNdAffineLayout ndAffine;
    } layout;
} AclsanDeviceMemoryAccessData;

// ==========================================
// ===========     SYNC CHECK      ==========
// ==========================================
typedef enum AclsanDeviceSyncKind {
    ACLSAN_DEVICE_SYNC_KIND_UNKNOWN = 0,
    ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG = 1,
    ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF = 2
} AclsanDeviceSyncKind;

typedef enum AclsanDeviceSyncAction {
    ACLSAN_DEVICE_SYNC_ACTION_UNKNOWN = 0,
    ACLSAN_DEVICE_SYNC_ACTION_SET = 1, // set / wait flag
    ACLSAN_DEVICE_SYNC_ACTION_WAIT = 2,
    ACLSAN_DEVICE_SYNC_ACTION_GET = 3, // get / rls buf
    ACLSAN_DEVICE_SYNC_ACTION_RELEASE = 4
} AclsanDeviceSyncAction;

typedef enum AclsanDeviceSyncScope {
    ACLSAN_DEVICE_SYNC_SCOPE_UNKNOWN = 0,
    ACLSAN_DEVICE_SYNC_SCOPE_PIPE = 1,
    ACLSAN_DEVICE_SYNC_SCOPE_BLOCK = 2,
    ACLSAN_DEVICE_SYNC_SCOPE_CORE = 3,
    ACLSAN_DEVICE_SYNC_SCOPE_CLUSTER = 4,
    ACLSAN_DEVICE_SYNC_SCOPE_DEVICE = 5,
    ACLSAN_DEVICE_SYNC_SCOPE_SOC = 6
} AclsanDeviceSyncScope;

typedef struct AclsanDeviceSyncData {
    uint64_t pc;
    uint64_t instrExecId;
    uint64_t launchId;
    uint32_t instrType;
    uint32_t blockId;
    uint32_t phyCoreId;
    uint32_t syncKind;
    uint32_t action;
    uint32_t scope;
    uint32_t srcPipe;
    uint32_t dstPipe;
    uint32_t mode;
    uint64_t objectId;
    uint32_t reserved[2];
} AclsanDeviceSyncData;

#endif
