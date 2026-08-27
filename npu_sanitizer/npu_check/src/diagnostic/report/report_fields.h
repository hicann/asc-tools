// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef ACLSAN_REPORT_FIELDS_H
#define ACLSAN_REPORT_FIELDS_H

#include "diagnostic/report_renderer.h"

namespace aclsan::cann::detail {

std::string FieldOr(const ReportFields& fields, const std::string& key, const char* fallback);
const char* MemorySpaceName(NpusanReportMemorySpace space);
const char* AccessModeName(NpusanReportAccessMode mode);
std::string Hex(std::uint64_t value);
std::string HexWithPrefix(std::uint64_t value);
std::string OrUnknown(const std::string& value);
std::string FormatCoreId(std::uint32_t coreId);
std::string FormatBlockType(std::uint32_t blockType);
std::string FormatLocation(const NpusanReportExecContext& exec, bool includeAt);
std::string FormatLocation(const ReportFrame& frame, bool includeAt);

void PutExecFields(const NpusanReportExecContext& exec, ReportFields* fields);
void PutPrefixedExecFields(const NpusanReportExecContext& exec, const std::string& prefix, ReportFields* fields);
void PutAccessFields(const NpusanReportMemoryAccess& access, ReportFields* fields);
void PutAllocationFields(const NpusanReportAllocation& allocation, ReportFields* fields);
void PutDefaultHostFields(ReportFields* fields);
void PutFrameLocationFields(const ReportFrame& frame, const std::string& prefix, ReportFields* fields);
void PutFaultLocationFields(const NpusanReportCommon& common, ReportFields* fields);

std::vector<ReportCallStack> ActiveCallStacks(const NpusanReportCommon& common);
const ReportCallStack* FindStackByRole(const NpusanReportCommon& common, ReportStackRole role);
const ReportFrame* FirstStructuredFrame(const NpusanReportCommon& common, ReportStackRole role);

} // namespace aclsan::cann::detail

#endif
