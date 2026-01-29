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
 * \file kernel_fp16.cpp
 * \brief
 */
#include "kernel_fp16.h"

namespace {
constexpr uint16_t K_MAN_BIT_LENGTH = 11;
}

// namespace float16 {
/**
 * @ingroup half global filed
 * @brief   round mode of last valid digital
 */
const enum TagFp16RoundMode ROUND_MODE = TagFp16RoundMode::K_ROUND_TO_NEAREST;

void ExtractFp16(const uint16_t& val, uint16_t& s, int16_t& e, uint16_t& m)
{
    // 1.Extract
    s = FP16_EXTRAC_SIGN(val);
    e = FP16_EXTRAC_EXP(val);
    m = FP16_EXTRAC_MAN(val);
    // Denormal
    if (e == 0) {
        e = 1;
    }
}

/**
 * @ingroup half static method
 * @param [in] man       truncated mantissa
 * @param [in] shiftOut left shift bits based on ten bits
 * @brief   judge whether to add one to the result while converting half to
 * other datatype
 * @return  Return true if add one, otherwise false
 */
static bool IsRoundOne(uint64_t man, uint16_t truncLen)
{
    uint64_t mask0 = 0x4;
    uint64_t mask1 = 0x2;
    uint64_t mask2;
    uint16_t shiftOut = static_cast<uint16_t>(truncLen - static_cast<uint16_t>(DimIndex::K_DIM2));
    mask0 = mask0 << shiftOut;
    mask1 = mask1 << shiftOut;
    mask2 = mask1 - 1;

    bool lastBit = ((man & mask0) > 0);
    bool truncHigh = false;
    bool truncLeft = false;
    if (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) {
        truncHigh = ((man & mask1) > 0);
        truncLeft = ((man & mask2) > 0);
    }
    return (truncHigh && (truncLeft || lastBit));
}

/**
 * @ingroup half public method
 * @param [in] exp       exponent of half value
 * @param [in] man       exponent of half value
 * @brief   normalize half value
 * @return
 */
static void Fp16Normalize(int16_t& exp, uint16_t& man)
{
    // set to invalid data
    if (exp >= static_cast<int16_t>(Fp16BasicParam::K_FP16_MAX_EXP)) {
        exp = static_cast<int16_t>(Fp16BasicParam::K_FP16_MAX_EXP);
        man = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_MAN);
    } else if ((exp == 0) && (man == static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT))) {
        exp++;
        man = 0;
    }
}

/**
 * @ingroup half math conversion static method
 * @param [in] fpVal uint16_t value of half object
 * @brief   Convert half to float/fp32
 * @return  Return float/fp32 value of fpVal which is the value of half object
 */
static float Fp16ToFloat(const uint16_t& fpVal)
{
    uint16_t hfSign;
    uint16_t hfMan;
    int16_t hfExp;
    ExtractFp16(fpVal, hfSign, hfExp, hfMan);

    while ((hfMan != 0) && ((hfMan & static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT)) == 0)) {
        hfMan <<= 1;
        hfExp--;
    }

    uint32_t eRet;
    uint32_t mRet;
    uint32_t sRet = hfSign;
    if (hfMan == 0) {
        eRet = 0;
        mRet = 0;
    } else {
        eRet = (static_cast<uint32_t>(hfExp) - static_cast<uint32_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) +
            static_cast<uint32_t>(Fp32BasicParam::K_FP32_EXP_BIAS);
        mRet = static_cast<uint32_t>(hfMan & static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_MASK));
        mRet = mRet << (static_cast<uint32_t>(Fp32BasicParam::K_FP32_MAN_LEN) -
            static_cast<uint32_t>(Fp16BasicParam::K_FP16_MAN_LEN));
    }
    uint32_t fVal = FP32_CONSTRUCTOR(sRet, eRet, mRet);
    auto pRetV = reinterpret_cast<float*>(&fVal);

    return *pRetV;
}

/**
 * @ingroup half math conversion static method
 * @param [in] fpVal uint16_t value of half object
 * @brief   Convert half to double/fp64
 * @return  Return double/fp64 value of fpVal which is the value of half object
 */
static double Fp16ToDouble(const uint16_t& fpVal)
{
    uint16_t hfSign;
    uint16_t hfMan;
    int16_t hfExp;
    ExtractFp16(fpVal, hfSign, hfExp, hfMan);

    while ((hfMan != 0) && ((hfMan & static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT)) == 0)) {
        hfMan <<= 1;
        hfExp--;
    }

    uint64_t eRet;
    uint64_t mRet;
    if (hfMan == 0) {
        eRet = 0;
        mRet = 0;
    } else {
        eRet = (static_cast<uint64_t>(hfExp) - static_cast<uint64_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) +
            static_cast<uint64_t>(Fp64BasicParam::K_FP64_EXP_BIAS);
        mRet = hfMan & static_cast<uint64_t>(Fp16BasicParam::K_FP16_MAN_MASK);
        mRet = mRet << (static_cast<uint64_t>(Fp64BasicParam::K_FP64_MAN_LEN) -
            static_cast<uint64_t>(Fp16BasicParam::K_FP16_MAN_LEN));
    }
    uint64_t fVal = (static_cast<uint64_t>(hfSign) << static_cast<uint64_t>(Fp64BasicParam::K_FP64_SIGN_INDEX)) |
        (eRet << static_cast<uint64_t>(Fp64BasicParam::K_FP64_MAN_LEN)) | (mRet);
    auto pRetV = reinterpret_cast<double*>(&fVal);

    return *pRetV;
}

// / @ingroup half static method
// / @param [in] sRet       sign of half value
// / @param [in] longIntM   man uint64_t value of half object
// / @param [in] shiftOut   shift offset
// / @brief   calculate uint8 value by sign,man and shift offset
// / @return Return uint8 value of half object
static uint8_t GetUint8ValByMan(uint8_t sRet, const uint64_t& longIntM, const uint16_t& shiftOut)
{
    bool needRound = IsRoundOne(longIntM, shiftOut + static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN));
    auto mRet = static_cast<uint8_t>(
        (longIntM >> (static_cast<uint32_t>(Fp16BasicParam::K_FP16_MAN_LEN) + static_cast<uint32_t>(shiftOut))) &
        static_cast<uint8_t>(NumBitMax::K_BIT_LEN8_MAX));
    needRound = needRound && (((sRet == 0) && (mRet < static_cast<uint8_t>(NumBitMax::K_INT8_MAX))) ||
        ((sRet == 1) && (mRet <= static_cast<uint8_t>(NumBitMax::K_INT8_MAX))));
    if (needRound) {
        mRet++;
    }
    if (sRet != 0) {
        mRet = (~mRet) + 1;
    }
    if (mRet == 0) {
        sRet = 0;
    }
    return static_cast<uint8_t>((sRet << static_cast<uint16_t>(BitShift::K_BIT_SHIFT7)) | (mRet));
}

static void CalcOverflowFlagInt8(const uint8_t& sRet, uint16_t& hfE, uint64_t& longIntM, uint8_t& overflowFlag,
    uint16_t& shiftOut)
{
    while (hfE != static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
        if (hfE > static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
            hfE--;
            longIntM = longIntM << 1;
            if ((sRet == 1) && (longIntM >= 0x20000u)) { // sign=1,negative number(<0)
                longIntM = 0x20000u;                     // 10 0000 0000 0000 0000  10(half-man)+7(int8)=17bit
                overflowFlag = 1;
                break;
            } else if ((sRet != 1) && (longIntM >= 0x1FFFFu)) { // sign=0,positive number(>0)
                longIntM = 0x1FFFFu;                            // 01 1111 1111 1111 1111  10(half-man)+7(int8)
                overflowFlag = 1;
                break;
            }
        } else {
            hfE++;
            shiftOut++;
        }
    }
}

/**
 * @ingroup half math conversion static method
 * @param [in] fpVal uint16_t value of half object
 * @brief   Convert half to int8_t
 * @return  Return int8_t value of fpVal which is the value of half object
 */
static int8_t Fp16ToInt8(const uint16_t& fpVal)
{
    uint8_t ret;
    // 1.get sRet and shift it to bit0.
    uint8_t sRet = static_cast<uint8_t>(FP16_EXTRAC_SIGN(fpVal));
    // 2.get hfE and hfM
    uint16_t hfE = static_cast<uint16_t>(FP16_EXTRAC_EXP(fpVal));
    uint16_t hfM = FP16_EXTRAC_MAN(fpVal);

    if (FP16_IS_DENORM(fpVal)) { // Denormalized number
        return 0;
    }

    uint64_t longIntM = hfM;
    uint8_t overflowFlag = 0;
    uint16_t shiftOut = 0;
    if (FP16_IS_INVALID(fpVal)) { // Inf or NaN
        overflowFlag = 1;
    } else {
        CalcOverflowFlagInt8(sRet, hfE, longIntM, overflowFlag, shiftOut);
    }
    if (overflowFlag != 0) {
        ret = static_cast<uint8_t>(NumBitMax::K_INT8_MAX) + sRet;
    } else {
        // Generate final result
        ret = GetUint8ValByMan(sRet, longIntM, shiftOut);
    }

    return static_cast<int8_t>(ret);
}

static void CalcOverflowFlagUInt8(uint16_t& hfE, uint64_t& longIntM, uint8_t& overflowFlag, uint8_t& mRet,
    uint16_t& shiftOut)
{
    while (hfE != static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
        if (hfE > static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
            hfE--;
            longIntM = longIntM << 1;
            if (longIntM >= 0x40000Lu) { // overflow 0100 0000 0000 0000 0000
                longIntM = 0x3FFFFLu;    // 11 1111 1111 1111 1111   10(half-man)+8(uint8)=18bit
                overflowFlag = 1;
                mRet = ~0;
                break;
            }
        } else {
            hfE++;
            shiftOut++;
        }
    }
}

/**
 * @ingroup half math conversion static method
 * @param [in] fpVal uint16_t value of half object
 * @brief   Convert half to uint8_t
 * @return  Return uint8_t value of fpVal which is the value of half object
 */
static uint8_t Fp16ToUInt8(const uint16_t& fpVal)
{
    uint8_t mRet = 0;
    // 1.get sRet and shift it to bit0.
    uint16_t sRet = FP16_EXTRAC_SIGN(fpVal);
    // 2.get hfE and hfM
    uint16_t hfE = static_cast<uint16_t>(FP16_EXTRAC_EXP(fpVal));
    uint16_t hfM = FP16_EXTRAC_MAN(fpVal);

    if (FP16_IS_DENORM(fpVal)) { // Denormalized number
        return 0;
    }

    if (FP16_IS_INVALID(fpVal)) { // Inf or NaN
        mRet = ~0;
    } else {
        uint64_t longIntM = hfM;
        uint8_t overflowFlag = 0;
        uint16_t shiftOut = 0;
        CalcOverflowFlagUInt8(hfE, longIntM, overflowFlag, mRet, shiftOut);
        if (overflowFlag == 0) {
            bool needRound = IsRoundOne(longIntM, shiftOut + static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN));
            mRet = static_cast<uint8_t>((longIntM >>
                (static_cast<uint32_t>(Fp16BasicParam::K_FP16_MAN_LEN) + static_cast<uint32_t>(shiftOut))) &
                static_cast<uint8_t>(NumBitMax::K_BIT_LEN8_MAX));
            if (needRound && (mRet != static_cast<uint8_t>(NumBitMax::K_BIT_LEN8_MAX))) {
                mRet++;
            }
        }
    }

    if (sRet == 1) { // Negative number
        mRet = 0;
    }
    // mRet equal to final result
    return mRet;
}
// / @ingroup half static method
// / @param [in] sRet       sign of half value
// / @param [in] longIntM   man uint64_t value of half object
// / @param [in] shiftOut   shift offset
// / @brief   calculate uint16 value by sign,man and shift offset
// / @return Return uint16 value of half object
static uint16_t GetUint16ValByMan(uint16_t sRet, const uint64_t& longIntM, const uint16_t& shiftOut)
{
    bool needRound = IsRoundOne(longIntM, shiftOut + static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN));
    auto mRet = static_cast<uint16_t>(
        (longIntM >> (static_cast<uint32_t>(Fp16BasicParam::K_FP16_MAN_LEN) + static_cast<uint32_t>(shiftOut))) &
        static_cast<uint16_t>(NumBitMax::K_BIT_LEN16_MAX));
    if (needRound && (mRet < static_cast<int16_t>(NumBitMax::K_INT16_MAX))) {
        mRet++;
    }
    if (sRet != 0) {
        mRet = (~mRet) + 1;
    }
    if (mRet == 0) {
        sRet = 0;
    }
    return static_cast<uint16_t>((sRet << static_cast<uint16_t>(BitShift::K_BIT_SHIFT15)) | (mRet));
}

static void CalcOverflowFlagInt16(uint16_t& hfE, uint64_t& longIntM, const uint16_t& sRet, uint8_t& overflowFlag,
    uint16_t& shiftOut)
{
    while (hfE != static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
        if (hfE > static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
            longIntM = longIntM << 1;
            hfE--;
            if ((sRet == 1) && (longIntM > 0x2000000Lu)) { // sign=1,negative number(<0)
                longIntM = 0x2000000Lu;                    // 10(half-man)+15(int16)=25bit
                overflowFlag = 1;
                break;
            } else if ((sRet != 1) && (longIntM >= 0x1FFFFFFLu)) { // sign=0,positive number(>0) Overflow
                longIntM = 0x1FFFFFFLu;                            // 10(half-man)+15(int16)=25bit
                overflowFlag = 1;
                break;
            }
        } else {
            shiftOut++;
            hfE++;
        }
    }
}
/**
 * @ingroup half math conversion static method
 * @param [in] fpVal uint16_t value of half object
 * @brief   Convert half to int16_t
 * @return  Return int16_t value of fpVal which is the value of half object
 */
static int16_t Fp16ToInt16(const uint16_t& fpVal)
{
    int16_t ret;
    uint16_t retV;
    // 1.get sRet and shift it to bit0.
    uint16_t sRet = FP16_EXTRAC_SIGN(fpVal);
    // 2.get hfE and hfM
    uint16_t hfM = FP16_EXTRAC_MAN(fpVal);
    uint16_t hfE = static_cast<uint16_t>(FP16_EXTRAC_EXP(fpVal));

    if (FP16_IS_DENORM(fpVal)) { // Denormalized number
        retV = 0;
        ret = *(reinterpret_cast<uint8_t*>(&retV));
        return ret;
    }

    uint8_t overflowFlag = 0;
    uint16_t shiftOut = 0;
    uint64_t longIntM = hfM;
    if (FP16_IS_INVALID(fpVal)) { // Inf or NaN
        overflowFlag = 1;
    } else {
        CalcOverflowFlagInt16(hfE, longIntM, sRet, overflowFlag, shiftOut);
    }
    if (overflowFlag != 0) {
        retV = static_cast<int16_t>(NumBitMax::K_INT16_MAX) + sRet;
    } else {
        // Generate final result
        retV = GetUint16ValByMan(sRet, longIntM, shiftOut);
    }
    ret = *(reinterpret_cast<int16_t*>(&retV));
    return ret;
}

/**
 * @ingroup half math conversion static method
 * @param [in] fpVal uint16_t value of half object
 * @brief   Convert half to uint16_t
 * @return  Return uint16_t value of fpVal which is the value of half object
 */
static uint16_t Fp16ToUInt16(const uint16_t& fpVal)
{
    uint16_t mRet;
    // 1.get sRet and shift it to bit0.
    uint16_t sRet = FP16_EXTRAC_SIGN(fpVal);
    // 2.get hfE and hfM
    int16_t hfE = FP16_EXTRAC_EXP(fpVal);
    uint16_t hfM = FP16_EXTRAC_MAN(fpVal);

    if (FP16_IS_DENORM(fpVal)) { // Denormalized number
        return 0;
    }

    if (FP16_IS_INVALID(fpVal)) { // Inf or NaN
        mRet = ~0;
    } else {
        uint16_t shiftOut = 0;
        uint64_t longIntM = hfM;
        while (hfE != static_cast<int16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
            if (hfE > static_cast<int16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
                hfE--;
                longIntM = longIntM << 1;
            } else {
                shiftOut++;
                hfE++;
            }
        }
        bool needRound = IsRoundOne(longIntM, shiftOut + static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN));
        mRet = static_cast<uint16_t>(
            (longIntM >> (static_cast<uint32_t>(Fp16BasicParam::K_FP16_MAN_LEN) + static_cast<uint32_t>(shiftOut))) &
            static_cast<uint16_t>(NumBitMax::K_BIT_LEN16_MAX));
        if (needRound && (mRet != static_cast<uint16_t>(NumBitMax::K_BIT_LEN16_MAX))) {
            mRet++;
        }
    }

    if (sRet == 1) { // Negative number
        mRet = 0;
    }
    // mRet equal to final result
    return mRet;
}

/**
 * @ingroup half math convertion static method
 * @param [in] fpVal uint16_t value of half object
 * @brief   Convert half to int32_t
 * @return  Return int32_t value of fpVal which is the value of half object
 */
static int32_t Fp16ToInt32(const uint16_t& fpVal)
{
    uint32_t retV;
    // 1.get sRet and shift it to bit0.
    uint32_t sRet = FP16_EXTRAC_SIGN(fpVal);
    // 2.get hfE and hfM
    int16_t hfE = FP16_EXTRAC_EXP(fpVal);
    uint16_t hfM = FP16_EXTRAC_MAN(fpVal);

    if (FP16_IS_INVALID(fpVal)) { // Inf or NaN
        retV = static_cast<int32_t>(NumBitMax::K_INT32_MAX) + sRet;
    } else {
        uint64_t longIntM = hfM;
        uint16_t shiftOut = 0;

        while (hfE != static_cast<int16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
            if (hfE > static_cast<int16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
                longIntM = longIntM << 1;
                hfE--;
            } else {
                hfE++;
                shiftOut++;
            }
        }
        bool needRound = IsRoundOne(longIntM, shiftOut + static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN));
        auto mRet = static_cast<uint32_t>(
            (longIntM >> (static_cast<uint32_t>(Fp16BasicParam::K_FP16_MAN_LEN) + static_cast<uint32_t>(shiftOut))) &
            static_cast<uint32_t>(NumBitMax::K_BIT_LEN32_MAX));
        if (needRound && (mRet < static_cast<uint32_t>(NumBitMax::K_INT32_MAX))) {
            mRet++;
        }

        if (sRet == 1) {
            mRet = (~mRet) + 1;
        }
        if (mRet == 0) {
            sRet = 0;
        }
        // Generate final result
        retV = (sRet << static_cast<uint16_t>(BitShift::K_BIT_SHIFT31)) | (mRet);
    }

    return *(reinterpret_cast<int32_t*>(&retV));
}

/**
 * @ingroup half math conversion static method
 * @param [in] fpVal uint16_t value of half object
 * @brief   Convert half to uint32_t
 * @return  Return uint32_t value of fpVal which is the value of half object
 */
static uint32_t Fp16ToUInt32(const uint16_t& fpVal)
{
    uint32_t mRet;
    // 1.get sRet and shift it to bit0.
    uint32_t sRet = FP16_EXTRAC_SIGN(fpVal);
    // 2.get hfE and hfM
    int16_t hfE = FP16_EXTRAC_EXP(fpVal);
    uint16_t hfM = FP16_EXTRAC_MAN(fpVal);

    if (FP16_IS_DENORM(fpVal)) { // Denormalized number
        return 0u;
    }

    if (FP16_IS_INVALID(fpVal)) { // Inf or NaN
        mRet = ~0u;
    } else {
        uint64_t longIntM = hfM;
        uint16_t shiftOut = 0;
        while (hfE != static_cast<int16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
            if (hfE > static_cast<int16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) {
                hfE--;
                longIntM = longIntM << 1;
            } else {
                hfE++;
                shiftOut++;
            }
        }
        bool needRound = IsRoundOne(longIntM, shiftOut + static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN));
        mRet = static_cast<uint32_t>(longIntM >>
            (static_cast<uint32_t>(Fp16BasicParam::K_FP16_MAN_LEN) + static_cast<uint32_t>(shiftOut))) &
            static_cast<uint32_t>(NumBitMax::K_BIT_LEN32_MAX);
        if (needRound && (mRet != static_cast<uint32_t>(NumBitMax::K_BIT_LEN32_MAX))) {
            mRet++;
        }
    }

    if (sRet == 1) { // Negative number
        mRet = 0;
    }
    // mRet equal to final result
    return mRet;
}
static uint16_t Fp16AddCalVal(const uint16_t& sRet, int16_t eRet, uint16_t mRet, uint32_t mTrunc, uint16_t shiftOut)
{
    uint16_t mMin = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT) << shiftOut;
    uint16_t mMax = mMin << 1;
    // Denormal
    while ((mRet < mMin) && (eRet > 0)) { // the value of mRet should not be smaller than 2^23
        mRet = mRet << 1;
        mRet += (static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK) & mTrunc) >>
            static_cast<uint16_t>(Fp32BasicParam::K_FP32_SIGN_INDEX);
        mTrunc = mTrunc << 1;
        eRet = eRet - 1;
    }
    while (mRet >= mMax) { // the value of mRet should be smaller than 2^24
        mTrunc = mTrunc >> 1;
        mTrunc = mTrunc | (static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK) * (mRet & 1));
        mRet = mRet >> 1;
        eRet = eRet + 1;
    }

    bool bLastBit = ((mRet & 1) > 0);
    bool bTruncHigh = (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) &&
        ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK)) > 0);
    bool bTruncLeft = (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) &&
        ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_ABS_MAX)) > 0);
    mRet = ManRoundToNearest(bLastBit, bTruncHigh, bTruncLeft, mRet, shiftOut);
    while (mRet >= mMax) {
        mRet = mRet >> 1;
        eRet = eRet + 1;
    }

    if ((eRet == 0) && (mRet <= mMax)) {
        mRet = mRet >> 1;
    }
    Fp16Normalize(eRet, mRet);
    uint16_t ret = FP16_CONSTRUCTOR(sRet, static_cast<uint16_t>(eRet), mRet);
    return ret;
}

/**
 * @ingroup half math operator
 * @param [in] v1 left operator value of half object
 * @param [in] v2 right operator value of half object
 * @brief   Performing half addition
 * @return  Return half result of adding this and fp
 */
static uint16_t Fp16Add(uint16_t v1, uint16_t v2)
{
    uint16_t sa;
    uint16_t sb;
    int16_t ea;
    int16_t eb;
    uint32_t ma;
    uint32_t mb;
    uint16_t maTmp;
    uint16_t mbTmp;
    uint16_t shiftOut = 0;
    // 1.Extract
    ExtractFp16(v1, sa, ea, maTmp);
    ExtractFp16(v2, sb, eb, mbTmp);
    ma = maTmp;
    mb = mbTmp;

    uint16_t sum;
    uint16_t sRet;
    if (sa != sb) {
        ReverseMan(sa > 0, ma);
        ReverseMan(sb > 0, mb);
        sum = static_cast<uint16_t>(GetManSum(ea, ma, eb, mb));
        sRet = (sum & static_cast<uint16_t>(Fp16BasicParam::K_FP16_SIGN_MASK)) >>
            static_cast<uint16_t>(Fp16BasicParam::K_FP16_SIGN_INDEX);
        ReverseMan(sRet > 0, ma);
        ReverseMan(sRet > 0, mb);
    } else {
        sum = static_cast<uint16_t>(GetManSum(ea, ma, eb, mb));
        sRet = sa;
    }

    if (sum == 0) {
        shiftOut = 3; // shift to left 3 bits
        ma = ma << shiftOut;
        mb = mb << shiftOut;
    }

    uint32_t mTrunc = 0;
    int16_t eRet = std::max(ea, eb);
    uint32_t eTmp = static_cast<uint32_t>(std::abs(ea - eb));
    if (ea > eb) {
        mTrunc = (mb << (static_cast<uint32_t>(BitShift::K_BIT_SHIFT32) - eTmp));
        mb = RightShift(mb, eTmp);
    } else if (ea < eb) {
        mTrunc = (ma << (static_cast<uint32_t>(BitShift::K_BIT_SHIFT32) - eTmp));
        ma = RightShift(ma, eTmp);
    }
    // calculate mantissav
    auto mRet = static_cast<uint16_t>(ma + mb);
    return Fp16AddCalVal(sRet, eRet, mRet, mTrunc, shiftOut);
}

/**
 * @ingroup half math operator
 * @param [in] v1 left operator value of half object
 * @param [in] v2 right operator value of half object
 * @brief   Performing half subtraction
 * @return  Return half result of subtraction fp from this
 */
static uint16_t Fp16Sub(uint16_t v1, uint16_t v2)
{
    // Reverse
    uint16_t tmp = ((~(v2)) & static_cast<uint16_t>(Fp16BasicParam::K_FP16_SIGN_MASK)) |
        (v2 & static_cast<uint16_t>(Fp16BasicParam::K_FP16_ABS_MAX));
    return Fp16Add(v1, tmp);
}
/**
 * @ingroup half math operator
 * @param [in] v1 left operator value of half object
 * @param [in] v2 right operator value of half object
 * @brief   Performing half multiplication
 * @return  Return half result of multiplying this and fp
 */
static uint16_t Fp16Mul(uint16_t v1, uint16_t v2)
{
    uint16_t sa;
    uint16_t sb;
    int16_t ea;
    int16_t eb;
    uint16_t mRet;
    uint16_t maTmp;
    uint16_t mbTmp;
    // 1.Extract
    ExtractFp16(v1, sa, ea, maTmp);
    ExtractFp16(v2, sb, eb, mbTmp);
    uint32_t ma = maTmp;
    uint32_t mb = mbTmp;

    int16_t eRet =
        (ea + eb - static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS)) - static_cast<uint16_t>(DimIndex::K_DIM10);
    uint32_t mulM = ma * mb;
    uint16_t sRet = sa ^ sb;

    uint32_t mMin = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT);
    uint32_t mMax = mMin << 1;
    uint32_t mTrunc = 0;
    // the value of mRet should not be smaller than 2^23
    while ((mulM < mMin) && (eRet > 1)) {
        mulM = mulM << 1;
        eRet = eRet - 1;
    }
    while ((mulM >= mMax) || (eRet < 1)) {
        mTrunc = mTrunc >> 1;
        mTrunc = mTrunc | (static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK) * (mulM & 1));
        mulM = mulM >> 1;
        eRet = eRet + 1;
    }
    bool bLastBit = ((mulM & 1) > 0);
    bool bTruncHigh = (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) &&
        ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK)) > 0);
    bool bTruncLeft = (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) &&
        ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_ABS_MAX)) > 0);
    mulM = ManRoundToNearest(bLastBit, bTruncHigh, bTruncLeft, mulM);

    while ((mulM >= mMax) || (eRet < 0)) {
        mulM = mulM >> 1;
        eRet = eRet + 1;
    }

    if ((eRet == 1) && (mulM < static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT))) {
        eRet = 0;
    }
    mRet = static_cast<uint16_t>(mulM);

    Fp16Normalize(eRet, mRet);

    uint16_t ret = FP16_CONSTRUCTOR(sRet, static_cast<uint16_t>(eRet), mRet);
    return ret;
}

/**
 * @ingroup half math operator divided
 * @param [in] v1 left operator value of half object
 * @param [in] v2 right operator value of half object
 * @brief   Performing half division
 * @return  Return half result of division this by fp
 */
static uint16_t Fp16Div(uint16_t v1, uint16_t v2)
{
    uint16_t ret;
    if (FP16_IS_ZERO(v2)) { // result is inf
        // throw "half division by zero.";
        uint16_t sa;
        uint16_t sb;
        uint16_t sRet;
        sa = FP16_EXTRAC_SIGN(v1);
        sb = FP16_EXTRAC_SIGN(v2);
        sRet = sa ^ sb;
        ret = FP16_CONSTRUCTOR(sRet, static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_EXP), 0u);
    } else if (FP16_IS_ZERO(v1)) {
        ret = 0u;
    } else {
        uint16_t sa;
        uint16_t sb;
        int16_t ea;
        int16_t eb;
        uint16_t maTmp;
        uint16_t mbTmp;
        // 1.Extract
        ExtractFp16(v1, sa, ea, maTmp);
        ExtractFp16(v2, sb, eb, mbTmp);
        uint64_t ma = maTmp;
        uint64_t mb = mbTmp;
        uint64_t mTmp;
        if (ea > eb) {
            mTmp = ma;
            int16_t tmp = ea - eb;
            for (int16_t i = 0; i < tmp; i++) {
                mTmp = mTmp << 1;
            }
            ma = mTmp;
        } else if (ea < eb) {
            mTmp = mb;
            int16_t tmp = eb - ea;
            for (int16_t i = 0; i < tmp; i++) {
                mTmp = mTmp << 1;
            }
            mb = mTmp;
        }
        float mDiv = static_cast<float>(ma * 1.0f / mb);
        half fpDiv(mDiv);
        ret = fpDiv.val;
        if (sa != sb) {
            ret |= static_cast<uint16_t>(Fp16BasicParam::K_FP16_SIGN_MASK);
        }
    }
    return ret;
}

// operate
half half::operator + (const half fp) const
{
    uint16_t retVal = Fp16Add(val, fp.val);
    half ret;
    ret.val = retVal;
    return ret;
}
half half::operator - (const half fp) const
{
    uint16_t retVal = Fp16Sub(val, fp.val);
    half ret;
    ret.val = retVal;
    return ret;
}
half half::operator*(const half fp) const
{
    uint16_t retVal = Fp16Mul(val, fp.val);
    half ret;
    ret.val = retVal;
    return ret;
}
half half::operator / (const half fp) const
{
    uint16_t retVal = Fp16Div(val, fp.val);
    half ret;
    ret.val = retVal;
    return ret;
}

half half::operator += (const half fp)
{
    val = Fp16Add(val, fp.val);
    return *this;
}
half half::operator -= (const half fp)
{
    val = Fp16Sub(val, fp.val);
    return *this;
}
half half::operator *= (const half fp)
{
    val = Fp16Mul(val, fp.val);
    return *this;
}
half half::operator /= (const half fp)
{
    val = Fp16Div(val, fp.val);
    return *this;
}

// compare
bool half::operator == (const half& fp) const
{
    bool result = true;
    if (FP16_IS_ZERO(val) && FP16_IS_ZERO(fp.val)) {
        result = true;
    } else {
        result = ((val & static_cast<uint16_t>(NumBitMax::K_BIT_LEN16_MAX)) ==
            (fp.val & static_cast<uint16_t>(NumBitMax::K_BIT_LEN16_MAX))); // bit compare
    }
    return result;
}
bool half::operator != (const half& fp) const
{
    bool result = true;
    if (FP16_IS_ZERO(val) && FP16_IS_ZERO(fp.val)) {
        result = false;
    } else {
        result = ((val & static_cast<uint16_t>(NumBitMax::K_BIT_LEN16_MAX)) !=
            (fp.val & static_cast<uint16_t>(NumBitMax::K_BIT_LEN16_MAX))); // bit compare
    }
    return result;
}
static bool CmpPosNums(const uint16_t& ea, const uint16_t& eb, const uint16_t& ma, const uint16_t& mb)
{
    bool result = true;
    if (ea > eb) { // ea - eb >= 1; Va always larger than Vb
        result = true;
    } else if (ea == eb) {
        result = ma > mb;
    } else {
        result = false;
    }
    return result;
}
static bool CmpNegNums(const uint16_t& ea, const uint16_t& eb, const uint16_t& ma, const uint16_t& mb)
{
    bool result = true;
    if (ea < eb) {
        result = true;
    } else if (ea == eb) {
        result = ma < mb;
    } else {
        result = false;
    }
    return result;
}
bool half::operator > (const half& fp) const
{
    uint16_t sa;
    uint16_t sb;
    uint16_t ea;
    uint16_t eb;
    uint16_t ma;
    uint16_t mb;
    bool result = true;

    // 1.Extract
    sa = FP16_EXTRAC_SIGN(val);
    sb = FP16_EXTRAC_SIGN(fp.val);
    ea = static_cast<uint16_t>(FP16_EXTRAC_EXP(val));
    eb = static_cast<uint16_t>(FP16_EXTRAC_EXP(fp.val));
    ma = FP16_EXTRAC_MAN(val);
    mb = FP16_EXTRAC_MAN(fp.val);

    // Compare
    if ((sa == 0) && (sb > 0)) { // +  -
        // -0=0
        result = !(FP16_IS_ZERO(val) && FP16_IS_ZERO(fp.val));
    } else if ((sa == 0) && (sb == 0)) { // + +
        result = CmpPosNums(ea, eb, ma, mb);
    } else if ((sa > 0) && (sb > 0)) { // - -    opposite to  + +
        result = CmpNegNums(ea, eb, ma, mb);
    } else {
        // -  +
        result = false;
    }

    return result;
}

bool half::operator >= (const half& fp) const
{
    bool result = true;
    if (((*this) > fp) || ((*this) == fp)) {
        result = true;
    } else {
        result = false;
    }

    return result;
}

bool half::operator <= (const half& fp) const
{
    bool result = true;
    if ((*this) > fp) {
        result = false;
    }
    return result;
}

bool half::operator < (const half& fp) const
{
    bool result = true;
    if ((*this) >= fp) {
        result = false;
    }
    return result;
}

half half::operator ++ ()
{
    half one = 1.0;
    val = Fp16Add(val, one.val);
    return *this;
}

half half::operator ++ (int)
{
    half oldBf = *this;
    operator++();
    return oldBf;
}

half half::operator -- ()
{
    half one = 1.0;
    val = Fp16Sub(val, one.val);
    return *this;
}

half half::operator -- (int)
{
    half oldBf = *this;
    operator--();
    return oldBf;
}

bool half::operator && (const half fp) const
{
    return (val != 0) && (fp.val != 0);
}

bool half::operator || (const half fp) const
{
    return (val != 0) || (fp.val != 0);
}

uint16_t half::FloatToFp16(const float& fVal) const
{
    uint16_t sRet;
    uint16_t mRet;
    int16_t eRet;
    uint32_t ef;
    uint32_t mf;
    const uint32_t ui32V = *(reinterpret_cast<const uint32_t*>(&fVal)); // 1:8:23bit sign:exp:man
    uint32_t mLenDelta;

    sRet = static_cast<uint16_t>((ui32V & static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK)) >>
        static_cast<uint16_t>(Fp32BasicParam::K_FP32_SIGN_INDEX)); // 4Byte->2Byte
    ef = (ui32V & static_cast<uint32_t>(Fp32BasicParam::K_FP32_EXP_MASK)) >>
        static_cast<uint16_t>(Fp32BasicParam::K_FP32_MAN_LEN); // 8 bit exponent
    mf = (ui32V &
        static_cast<uint32_t>(Fp32BasicParam::K_FP32_MAN_MASK)); // 23 bit mantissa dont't need to care about denormal
    mLenDelta =
        static_cast<uint16_t>(Fp32BasicParam::K_FP32_MAN_LEN) - static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN);

    bool needRound = false;
    // Exponent overflow/NaN converts to signed inf/NaN
    if (ef > 0x8Fu) { // 0x8Fu:142=127+15
        eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_EXP) - 1;
        mRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_MAN);
    } else if (ef <= 0x70u) { // 0x70u:112=127-15 Exponent underflow converts to denormalized half or signed zero
        eRet = 0;
        if (ef >= 0x67) { // 0x67:103=127-24 Denormal
            mf = (mf | static_cast<uint32_t>(Fp32BasicParam::K_FP32_MAN_HIDE_BIT));
            uint16_t shiftOut = static_cast<uint16_t>(Fp32BasicParam::K_FP32_MAN_LEN);
            uint64_t mTmp = (static_cast<uint64_t>(mf)) << (ef - 0x67);

            needRound = IsRoundOne(mTmp, shiftOut);
            mRet = static_cast<uint16_t>(mTmp >> shiftOut);
            if (needRound) {
                mRet++;
            }
        } else if ((ef == 0x66) && (mf > 0)) { // 0x66:102 Denormal 0<f_v<min(Denormal)
            mRet = 1;
        } else {
            mRet = 0;
        }
    } else { // Regular case with no overflow or underflow
        eRet = static_cast<int16_t>(ef - 0x70u);

        needRound = IsRoundOne(mf, static_cast<uint16_t>(mLenDelta));
        mRet = static_cast<uint16_t>(mf >> mLenDelta);
        if (needRound) {
            mRet++;
        }
        if ((mRet & static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT)) != 0) {
            eRet++;
        }
    }

    Fp16Normalize(eRet, mRet);
    return FP16_CONSTRUCTOR(sRet, static_cast<uint16_t>(eRet), mRet);
}

uint16_t half::DoubleToFp16(const double& dVal)
{
    uint16_t sRet;
    uint16_t mRet;
    int16_t eRet;
    uint64_t ed;
    uint64_t md;
    uint64_t ui64V = *(reinterpret_cast<const uint64_t*>(&dVal)); // 1:11:52bit sign:exp:man
    uint16_t mLenDelta;

    sRet = static_cast<uint16_t>((ui64V & static_cast<uint64_t>(Fp64BasicParam::K_FP64_SIGN_MASK)) >>
        static_cast<uint16_t>(Fp64BasicParam::K_FP64_SIGN_INDEX)); // 4Byte
    ed = (ui64V & static_cast<uint64_t>(Fp64BasicParam::K_FP64_EXP_MASK)) >>
        static_cast<uint16_t>(Fp64BasicParam::K_FP64_MAN_LEN);             // 10 bit exponent
    md = (ui64V & static_cast<uint64_t>(Fp64BasicParam::K_FP64_MAN_MASK)); // 52 bit mantissa
    mLenDelta =
        static_cast<uint16_t>(Fp64BasicParam::K_FP64_MAN_LEN) - static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN);

    bool needRound = false;
    // Exponent overflow/NaN converts to signed inf/NaN
    if (ed >= 0x410u) { // 0x410:1040=1023+16
        eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_EXP) - 1;
        mRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_MAN);
        val = FP16_CONSTRUCTOR(sRet, static_cast<uint16_t>(eRet), mRet);
    } else if (ed <= 0x3F0u) { // Exponent underflow converts to denormalized half or signed zero
        // 0x3F0:1008=1023-15
        // Signed zeros, denormalized floats, and floats with small
        // exponents all convert to signed zero half precision.
        eRet = 0;
        if (ed >= 0x3E7u) { // 0x3E7u:999=1023-24 Denormal
            // Underflows to a denormalized value
            md = (static_cast<uint64_t>(Fp64BasicParam::K_FP64_MAN_HIDE_BIT) | md);
            uint16_t shiftOut = static_cast<uint16_t>(Fp64BasicParam::K_FP64_MAN_LEN);
            uint64_t mTmp = (static_cast<uint64_t>(md)) << (ed - 0x3E7u);

            needRound = IsRoundOne(mTmp, shiftOut);
            mRet = static_cast<uint16_t>(mTmp >> shiftOut);
            if (needRound) {
                mRet++;
            }
        } else if ((ed == 0x3E6u) && (md > 0)) {
            mRet = 1;
        } else {
            mRet = 0;
        }
    } else { // Regular case with no overflow or underflow
        eRet = static_cast<int16_t>(ed - 0x3F0u);

        needRound = IsRoundOne(md, mLenDelta);
        mRet = static_cast<uint16_t>(md >> mLenDelta);
        if (needRound) {
            mRet++;
        }
        if ((static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT) & mRet) != 0) {
            eRet++;
        }
    }

    Fp16Normalize(eRet, mRet);
    return FP16_CONSTRUCTOR(sRet, static_cast<uint16_t>(eRet), mRet);
}

uint16_t half::Int8ToFp16(const int8_t& iVal) const
{
    uint16_t sRet;
    uint16_t eRet;
    uint16_t mRet;

    sRet = ((static_cast<uint8_t>(iVal)) & 0x80) == 0 ? 0 : 1;
    mRet = static_cast<uint16_t>(((static_cast<uint8_t>(iVal)) & static_cast<int8_t>(NumBitMax::K_INT8_MAX)));

    if (mRet == 0) {
        eRet = 0;
    } else {
        if (sRet != 0) {                                  // negative number(<0)
            mRet = static_cast<uint16_t>(std::abs(iVal)); // complement
        }

        eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN);
        while ((mRet & static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT)) == 0) {
            eRet = eRet - 1;
            mRet = mRet << 1;
        }
        eRet = eRet + static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS);
    }
    return FP16_CONSTRUCTOR(sRet, eRet, mRet);
}

uint16_t half::UInt8ToFp16(const uint8_t& uiVal) const
{
    uint16_t sRet;
    uint16_t eRet;
    uint16_t mRet;
    sRet = 0;
    eRet = 0;
    mRet = uiVal;
    if (mRet != 0) {
        eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN);
        while ((mRet & static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT)) == 0) {
            mRet = mRet << 1;
            eRet = eRet - 1;
        }
        eRet = eRet + static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS);
    }
    return FP16_CONSTRUCTOR(sRet, eRet, mRet);
}

static void SetValByUint16Val(const uint16_t& inputVal, const uint16_t& sign, uint16_t& retVal)
{
    uint32_t mTmp = (inputVal & static_cast<uint32_t>(Fp32BasicParam::K_FP32_ABS_MAX));
    uint16_t mMin = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT);
    uint16_t mMax = mMin << 1;
    uint16_t len = static_cast<uint16_t>(GetManBitLength(mTmp));
    if (mTmp != 0) {
        uint16_t eRet;
        if (len > static_cast<uint16_t>(DimIndex::K_DIM11)) {
            eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS) +
                static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN);
            uint32_t eTmp = static_cast<uint32_t>(len) - static_cast<uint32_t>(DimIndex::K_DIM11);
            uint32_t truncMask = 1;
            for (uint32_t i = 1; i < eTmp; i++) {
                truncMask = (truncMask << 1) + 1;
            }
            uint32_t mTrunc = (mTmp & truncMask) << (static_cast<uint32_t>(BitShift::K_BIT_SHIFT32) - eTmp);
            for (uint32_t i = 0; i < eTmp; i++) {
                eRet = eRet + 1;
                mTmp = (mTmp >> 1);
            }
            bool bLastBit = ((mTmp & 1) > 0);
            bool bTruncHigh = static_cast<bool>(0);
            bool bTruncLeft = static_cast<bool>(0);
            if (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) { // trunc
                bTruncHigh = ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK)) > 0);
                bTruncLeft = ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_ABS_MAX)) > 0);
            }
            mTmp = ManRoundToNearest(bLastBit, bTruncHigh, bTruncLeft, mTmp);
            while (mTmp >= mMax) {
                mTmp = mTmp >> 1;
                eRet = eRet + 1;
            }
        } else {
            eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS);
            mTmp = mTmp << static_cast<uint32_t>(K_MAN_BIT_LENGTH - len);
            eRet = eRet + (len - static_cast<uint16_t>(1));
        }
        auto mRet = static_cast<uint16_t>(mTmp);
        retVal = FP16_CONSTRUCTOR(sign, eRet, mRet);
    }
}

uint16_t half::Int16ToFp16(const int16_t& iVal) const
{
    uint16_t retVal = 0;
    if (iVal != 0) {
        uint16_t uiVal = *(reinterpret_cast<const uint16_t*>(&iVal));
        auto sRet = static_cast<uint16_t>(uiVal >> static_cast<uint16_t>(BitShift::K_BIT_SHIFT15));
        if (sRet != 0) {
            int16_t iValM = -iVal;
            uiVal = *(reinterpret_cast<uint16_t*>(&iValM));
        }
        SetValByUint16Val(uiVal, sRet, retVal);
    }
    return retVal;
}

uint16_t half::UInt16ToFp16(const uint16_t& uiVal)
{
    if (uiVal == 0) {
        return 0;
    } else {
        uint16_t eRet;
        uint16_t mRet = uiVal;
        uint16_t mMin = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT);
        uint16_t mMax = mMin << 1;
        uint16_t len = static_cast<uint16_t>(GetManBitLength(mRet));
        if (len > K_MAN_BIT_LENGTH) {
            eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS) +
                static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN);
            uint32_t mTrunc;
            uint32_t truncMask = 1;
            uint32_t eTmp = static_cast<uint32_t>(len - K_MAN_BIT_LENGTH);
            for (uint32_t i = 1; i < eTmp; i++) {
                truncMask = (truncMask << 1) + 1;
            }
            mTrunc = (mRet & truncMask) << (static_cast<uint32_t>(BitShift::K_BIT_SHIFT32) - eTmp);
            for (uint32_t i = 0; i < eTmp; i++) {
                mRet = (mRet >> 1);
                eRet = eRet + 1;
            }
            bool bLastBit = ((mRet & 1) > 0);
            bool bTruncHigh = static_cast<bool>(0);
            bool bTruncLeft = static_cast<bool>(0);
            if (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) { // trunc
                bTruncHigh = ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK)) > 0);
                bTruncLeft = ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_ABS_MAX)) > 0);
            }
            mRet = ManRoundToNearest(bLastBit, bTruncHigh, bTruncLeft, mRet);
            while (mRet >= mMax) {
                mRet = mRet >> 1;
                eRet = eRet + 1;
            }
            if (FP16_IS_INVALID(val)) {
                val = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX);
            }
        } else {
            eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS);
            mRet = mRet << (static_cast<uint32_t>(DimIndex::K_DIM11) - static_cast<uint32_t>(len));
            eRet = eRet + (len - static_cast<uint16_t>(1));
        }
        return FP16_CONSTRUCTOR(0u, eRet, mRet);
    }
}

static void SetValByUint32Val(const uint32_t& inputVal, const uint16_t& sign, uint16_t& retVal)
{
    uint16_t eRet;
    uint32_t mMin = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT);
    uint32_t mMax = mMin << 1;
    uint32_t mTmp = (inputVal & static_cast<uint32_t>(Fp32BasicParam::K_FP32_ABS_MAX));
    uint16_t len = static_cast<uint16_t>(GetManBitLength(mTmp));
    if (len > static_cast<uint16_t>(DimIndex::K_DIM11)) {
        eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS) +
            static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN);
        uint32_t truncMask = 1;
        uint32_t eTmp = len - static_cast<uint32_t>(DimIndex::K_DIM11);
        for (uint32_t i = 1; i < eTmp; i++) {
            truncMask = (truncMask << 1) + 1;
        }
        uint32_t mTrunc = (mTmp & truncMask) << (static_cast<uint32_t>(BitShift::K_BIT_SHIFT32) - eTmp);
        for (uint32_t i = 0; i < eTmp; i++) {
            mTmp = (mTmp >> 1);
            eRet = eRet + 1;
        }
        bool bLastBit = ((mTmp & 1) > 0);
        bool bTruncHigh = static_cast<bool>(0);
        bool bTruncLeft = static_cast<bool>(0);
        if (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) { // trunc
            bTruncLeft = ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_ABS_MAX)) > 0);
            bTruncHigh = ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK)) > 0);
        }
        mTmp = ManRoundToNearest(bLastBit, bTruncHigh, bTruncLeft, mTmp);
        while (mTmp >= mMax) {
            eRet = eRet + 1;
            mTmp = mTmp >> 1;
        }
        if (eRet >= static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_EXP)) {
            mTmp = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_MAN);
            eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_EXP) - 1;
        }
    } else {
        eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS);
        eRet = eRet + (len - 1);
        mTmp = mTmp << (static_cast<uint32_t>(DimIndex::K_DIM11) - static_cast<uint32_t>(len));
    }
    auto mRet = static_cast<uint16_t>(mTmp);
    retVal = FP16_CONSTRUCTOR(sign, eRet, mRet);
}

uint16_t half::Int32ToFp16(const int32_t& iVal) const
{
    uint16_t retVal = 0;
    if (iVal != 0) {
        uint32_t uiVal = *(reinterpret_cast<const uint32_t*>(&iVal));
        auto sRet = static_cast<uint16_t>(uiVal >> static_cast<uint16_t>(BitShift::K_BIT_SHIFT31));
        if (sRet != 0) {
            int32_t iValM = -iVal;
            uiVal = *(reinterpret_cast<uint32_t*>(&iValM));
        }
        SetValByUint32Val(uiVal, sRet, retVal);
    }
    return retVal;
}

uint16_t half::UInt32ToFp16(const uint32_t& uiVal) const
{
    if (uiVal == 0) {
        return 0;
    } else {
        uint16_t eRet;
        uint32_t mTmp = uiVal;
        uint32_t mMin = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_HIDE_BIT);
        uint32_t mMax = mMin << 1;
        uint16_t len = static_cast<uint16_t>(GetManBitLength(mTmp));
        if (len > static_cast<uint16_t>(DimIndex::K_DIM11)) {
            eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS) +
                static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAN_LEN);
            uint32_t truncMask = 1;
            uint16_t eTmp = len - static_cast<uint16_t>(DimIndex::K_DIM11);
            for (uint16_t i = 1; i < eTmp; i++) {
                truncMask = (truncMask << 1) + 1;
            }
            uint32_t mTrunc =
                (mTmp & truncMask) << static_cast<uint32_t>(static_cast<uint16_t>(BitShift::K_BIT_SHIFT32) - eTmp);
            for (uint16_t i = 0; i < eTmp; i++) {
                mTmp = (mTmp >> 1);
                eRet = eRet + 1;
            }
            bool bLastBit = ((mTmp & 1) > 0);
            bool bTruncHigh = false;
            bool bTruncLeft = false;
            if (ROUND_MODE == TagFp16RoundMode::K_ROUND_TO_NEAREST) { // trunc
                bTruncHigh = ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_SIGN_MASK)) > 0);
                bTruncLeft = ((mTrunc & static_cast<uint32_t>(Fp32BasicParam::K_FP32_ABS_MAX)) > 0);
            }
            mTmp = ManRoundToNearest(bLastBit, bTruncHigh, bTruncLeft, mTmp);
            while (mTmp >= mMax) {
                mTmp = mTmp >> 1;
                eRet = eRet + 1;
            }
            if (eRet >= static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_EXP)) {
                eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_EXP) - 1;
                mTmp = static_cast<uint16_t>(Fp16BasicParam::K_FP16_MAX_MAN);
            }
        } else {
            eRet = static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_BIAS);
            mTmp = mTmp << (static_cast<uint32_t>(DimIndex::K_DIM11) - static_cast<uint32_t>(len));
            eRet = eRet + (len - static_cast<uint16_t>(1));
        }
        auto mRet = static_cast<uint16_t>(mTmp);
        return FP16_CONSTRUCTOR(0u, eRet, mRet);
    }
}

// evaluation
half& half::operator = (const half& fp)
{
    if (&fp == this) {
        return *this;
    }
    val = fp.val;
    return *this;
}
half& half::operator = (const float& fVal)
{
    val = FloatToFp16(fVal);
    return *this;
}
half& half::operator = (const double& dVal)
{
    val = DoubleToFp16(dVal);
    return *this;
}
half& half::operator = (const int8_t& iVal)
{
    val = Int8ToFp16(iVal);
    return *this;
}
half& half::operator = (const uint8_t& uiVal)
{
    val = UInt8ToFp16(uiVal);
    return *this;
}
half& half::operator = (const int16_t& iVal)
{
    val = Int16ToFp16(iVal);
    return *this;
}
half& half::operator = (const uint16_t& uiVal)
{
    val = UInt16ToFp16(uiVal);
    return *this;
}
half& half::operator = (const int32_t& iVal)
{
    val = Int32ToFp16(iVal);
    return *this;
}
half& half::operator = (const uint32_t& uiVal)
{
    val = UInt32ToFp16(uiVal);
    return *this;
}

// convert
half::operator float() const
{
    return Fp16ToFloat(val);
}
half::operator double() const
{
    return Fp16ToDouble(val);
}
half::operator int8_t() const
{
    return Fp16ToInt8(val);
}
half::operator uint8_t() const
{
    return Fp16ToUInt8(val);
}
half::operator int16_t() const
{
    return Fp16ToInt16(val);
}
half::operator uint16_t() const
{
    return Fp16ToUInt16(val);
}
half::operator int32_t() const
{
    return Fp16ToInt32(val);
}
half::operator uint32_t() const
{
    return Fp16ToUInt32(val);
}

int32_t half::IsInf() const
{
    if (((val) & (static_cast<uint16_t>(Fp16BasicParam::K_FP16_ABS_MAX))) ==
        static_cast<uint16_t>(Fp16BasicParam::K_FP16_EXP_MASK)) {
        if (((static_cast<uint16_t>(Fp16BasicParam::K_FP16_SIGN_MASK)) & (val)) != 0) {
            return -1;
        } else {
            return 1;
        }
    } else {
        return 0;
    }
}

float half::ToFloat() const
{
    return Fp16ToFloat(val);
}
double half::ToDouble() const
{
    return Fp16ToDouble(val);
}
int8_t half::ToInt8() const
{
    return Fp16ToInt8(val);
}
uint8_t half::ToUInt8() const
{
    return Fp16ToUInt8(val);
}
int16_t half::ToInt16() const
{
    return Fp16ToInt16(val);
}
uint16_t half::ToUInt16() const
{
    return Fp16ToUInt16(val);
}
int32_t half::ToInt32() const
{
    return Fp16ToInt32(val);
}
uint32_t half::ToUInt32() const
{
    return Fp16ToUInt32(val);
}
// } // namespace float16
