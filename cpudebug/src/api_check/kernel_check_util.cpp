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
 * \file kernel_check_util.cpp
 * \brief
 */

#include "kernel_check_util.h"
#include "kernel_cpu_check.h"
#include "kernel_check_params.h"

namespace AscendC {
namespace check {

#define ASCENDC_CHECK_INTRI_NAME(intriName)                                                 \
    do {                                                                                    \
        if ((intriName == nullptr) || (intriName != nullptr && intriName[0] == '\0')) {     \
            CHECK_LOG_ERROR("intriName is null.");                                          \
            return false;                                                                   \
        }                                                                                   \
    } while (0)                                                                             \

bool CheckFuncCopyImplForMaskArray(CopyApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppCopyCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({mask[1], mask[0]})) {
        return false;
    }
    return true;
}

bool CheckFuncCopyImpl(CopyApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppCopyCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({mask})) {
        return false;
    }
    return true;
}

bool CheckFuncCopyImpl(CopyApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppCopyCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncDataCopyImpl(DataCopyApiParams &chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppDataCopyCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncDataCopyPadImpl(DataCopyPadApiParams &chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppDataCopyPadCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncDataCopySliceImpl(DataCopySliceApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppDataCopySliceCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncMmadImpl(MmadApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppMmadCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncVecBinaryImplForMaskArray(VecBinaryApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFuncVecBinaryImpl(VecBinaryApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFuncVecBinaryImpl(VecBinaryApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFuncVecSelectImplForMaskArray(VecSelectApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppSelectBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFuncVecSelectImpl(VecSelectApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppSelectBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFuncVecSelectImpl(VecSelectApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppSelectBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFuncVecBinaryCmpImplForMaskArray(VecBinaryApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFuncVecCmpRgtImplForMaskArray(VecCmpRgtApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecCmpRgtCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFuncVecCmpRgtImpl(VecCmpRgtApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecCmpRgtCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFuncVecBinaryCmpImpl(VecBinaryApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFuncVecBinaryCmpImpl(VecBinaryApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFunVecBinaryScalarImplForMaskArray(VecBinaryScalarApiParams& chkParams, const uint64_t mask[2],
    const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryScalarCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFunVecBinaryScalarImpl(VecBinaryScalarApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryScalarCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFunVecBinaryScalarImpl(VecBinaryScalarApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBinaryScalarCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFuncVecBinaryScalarCmpImpl(VecBinaryScalarApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecCompareScalarCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFuncVecBinaryScalarCmpImpl(VecBinaryScalarApiParams& chkParams, const uint64_t mask,
    const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecCompareScalarCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFunDupImplForMaskArray(VecDupApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecDupCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({mask[1], mask[0]})) {
        return false;
    }
    return true;
}

bool CheckFunDupImpl(VecDupApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecDupCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({mask})) {
        return false;
    }
    return true;
}

bool CheckFunDupImpl(VecDupApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecDupCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFunBcBImpl(VecBroadCastApiParams& chkParams, uint32_t dtypeSize, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    const uint64_t lowMask = FULL_MASK;
    const uint64_t highMask = (dtypeSize == 4) ? 0 : FULL_MASK;
    check::TikcppVecBroadCastCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({highMask, lowMask})) {
        return false;
    }
    return true;
}

bool CheckFunReduceOtherImpl(VecReduceApiParams &chkParams, const uint64_t mask, const char *intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecReduceOtherCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFunReduceOtherImplForMaskArray(VecReduceApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecReduceOtherCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFunReduceOtherWhlImpl(VecReduceWhlApiParams &chkParams, const uint64_t mask, const char *intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecReduceOtherWhlCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFunReduceOtherWhlImplForMaskArray(VecReduceWhlApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecReduceOtherWhlCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFunReduceImpl(VecReduceApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecReduceCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFunReduceImplMode2(VecReduceApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecReduceCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevelMode2();
}

bool CheckFunReduceImpl(VecReduceApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecReduceCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFunReduceImplForMaskArray(VecReduceApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecReduceCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFunScatterImpl(VecScatterApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecScatterCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFunScatterImpl(VecScatterApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecScatterCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFunScatterImplForMaskArray(VecScatterApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecScatterCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFuncGatherbImpl(VecGatherApiParams& chkParams, uint32_t dtypeSize, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    const uint64_t lowMask = (dtypeSize == 8) ? 0xffffffff : FULL_MASK;
    const uint64_t highMask = (dtypeSize >= 4) ? 0 : FULL_MASK;
    check::TikcppVecGatherbCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({highMask, lowMask});
}

bool CheckFuncGatherImpl(VecGatherApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecGatherCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFuncGatherImpl(VecGatherApiParams& chkParams, const uint64_t mask[], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecGatherCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFuncGatherImpl(VecGatherApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecGatherCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFuncCreateVecIndexImpl(VecCreateVecIndexApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecCreateVecIndexCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask[1], mask[0]});
}

bool CheckFuncCreateVecIndexImpl(VecCreateVecIndexApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecCreateVecIndexCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllLowLevel({mask});
}

bool CheckFuncCreateVecIndexImpl(VecCreateVecIndexApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecCreateVecIndexCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}


bool CheckFuncBilinearInterpolationImpl(VecBilinearInterpolationApiParams& chkParams, const uint64_t mask,
    const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBilinearInterpolationCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({mask})) {
        return false;
    }
    return true;
}

bool CheckFuncBilinearInterpolationImpl(VecBilinearInterpolationApiParams& chkParams, const uint64_t mask[2],
    const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecBilinearInterpolationCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({mask[1], mask[0]})) {
        return false;
    }
    return true;
}

bool CheckFuncInitConstValueImpl(CubeInitConstValueApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppCubeInitConstValueCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel()) {
        return false;
    }
    return true;
}

bool CheckFunTransposeImpl(VecTransposeApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecTransposeCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel()) {
        return false;
    }
    return true;
}

bool CheckFunProposalImpl(VecProposalApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecProposalCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckSortImpl(SortApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVecSortCheck chkIns{intriName, chkParams};
    return chkIns.CheckAllHighLevel();
}

bool CheckFuncLoadData2dImpl(LoadData2dApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppLoaddata2dCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncLoadData2dv2Impl(LoadData2dv2ApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppLoaddata2dv2Check chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncLoadData3dv1Impl(LoadData3dv1ApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppLoaddata3dv1Check chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncLoadData3dv2Impl(LoadData3dv2ApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppLoaddata3dv2Check chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncLoadData3dv2ProImpl(LoadData3dv2ProApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppLoaddata3dv2ProCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncLoadImageToLocalImpl(LoadImageToLocalApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppLoadImageToLocalCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncBroadCastToMMImpl(VecBroadCastToMMApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppBroadCastToMMCheck chkIns { intriName, chkParams };
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

bool CheckFuncVecGatherMaskImpl(VecGatherMaskApiParams& chkParams, const uint32_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppGatherMaskCheck chkIns { intriName, chkParams };
    if (!chkIns.CheckAllLowLevel({0, mask})) {
        return false;
    }
    return true;
}

bool CheckVectorPaddingForMaskArray(VectorPaddingApiParams& chkParams, const uint64_t mask[2], const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVectorPaddingCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({mask[1], mask[0]})) {
        return false;
    }
    return true;
}

bool CheckVectorPadding(VectorPaddingApiParams& chkParams, const uint64_t mask, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVectorPaddingCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllLowLevel({mask})) {
        return false;
    }
    return true;
}

bool CheckVectorPadding(VectorPaddingApiParams& chkParams, const char* intriName)
{
    ASCENDC_CHECK_INTRI_NAME(intriName);
    check::TikcppVectorPaddingCheck chkIns{intriName, chkParams};
    if (!chkIns.CheckAllHighLevel()) {
        return false;
    }
    return true;
}

uint64_t GetHardWarebufferSize(uint8_t index)
{
    return check::GlobalParams::Instance().bufferSizeMap.at(index);
}

}
}