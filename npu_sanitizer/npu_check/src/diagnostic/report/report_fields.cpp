// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/report/report_fields.h"

#include "aclsan/aclsan_cbdata_device.h"

#include <iomanip>
#include <sstream>

namespace aclsan::cann::detail {

const char* MemorySpaceName(NpusanReportMemorySpace space)
{
    switch (space) {
        case NpusanReportMemorySpace::GM:
            return "GM";
        case NpusanReportMemorySpace::UB:
            return "UB";
        case NpusanReportMemorySpace::L1:
            return "L1";
        case NpusanReportMemorySpace::L0_A:
            return "L0A";
        case NpusanReportMemorySpace::L0_B:
            return "L0B";
        case NpusanReportMemorySpace::L0_C:
            return "L0C";
        case NpusanReportMemorySpace::BT:
            return "BT";
        case NpusanReportMemorySpace::PRIVATE:
            return "private";
        case NpusanReportMemorySpace::HOST:
            return "host";
        case NpusanReportMemorySpace::UNKNOWN:
            return "unknown";
    }
    return "unknown";
}

const char* AccessModeName(NpusanReportAccessMode mode)
{
    switch (mode) {
        case NpusanReportAccessMode::READ:
            return "read";
        case NpusanReportAccessMode::WRITE:
            return "write";
        case NpusanReportAccessMode::READ_WRITE:
            return "read/write";
        case NpusanReportAccessMode::FREE:
            return "free";
    }
    return "access";
}

std::string FieldOr(const ReportFields& fields, const std::string& key, const char* fallback)
{
    const auto field = fields.find(key);
    return field == fields.end() || field->second.empty() ? fallback : field->second;
}

std::string Hex(std::uint64_t value)
{
    std::ostringstream os;
    os << std::hex << std::nouppercase << value;
    return os.str();
}

std::string HexWithPrefix(std::uint64_t value) { return std::string("0x") + Hex(value); }

std::string OrUnknown(const std::string& value) { return value.empty() ? "<unknown>" : value; }

std::string FormatCoreId(std::uint32_t coreId)
{
    return coreId == std::numeric_limits<std::uint32_t>::max() ? "<unknown>" : std::to_string(coreId);
}

std::string FormatBlockType(std::uint32_t blockType)
{
    switch (blockType) {
        case ACLSAN_DEVICE_BLOCK_TYPE_AICORE:
            return "AICORE";
        case ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR:
            return "AIV";
        case ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE:
            return "AIC";
        default:
            return "<unknown>";
    }
}

std::string FormatLocation(const NpusanReportExecContext& exec, bool includeAt)
{
    std::ostringstream os;
    if (includeAt) {
        os << "at ";
    }
    if (!exec.function.empty() || !exec.file.empty()) {
        os << OrUnknown(exec.function) << "+0x" << Hex(exec.offset) << " in " << OrUnknown(exec.file) << ":"
           << exec.line;
    } else {
        os << "pc 0x" << Hex(exec.pc) << " in " << OrUnknown(exec.kernelName);
    }
    return os.str();
}

std::string FormatLocation(const ReportFrame& frame, bool includeAt)
{
    std::ostringstream os;
    if (includeAt) {
        os << "at ";
    }
    os << OrUnknown(frame.function) << "+0x" << Hex(frame.offset) << " in " << OrUnknown(frame.file) << ":"
       << frame.line;
    return os.str();
}

void PutExecFields(const NpusanReportExecContext& exec, ReportFields* fields)
{
    (*fields)["function"] = OrUnknown(exec.function);
    (*fields)["offset"] = Hex(exec.offset);
    (*fields)["file"] = OrUnknown(exec.file);
    (*fields)["line"] = std::to_string(exec.line);
    (*fields)["pc"] = Hex(exec.pc);
    (*fields)["kernelName"] = OrUnknown(exec.kernelName);
    (*fields)["coreId"] = FormatCoreId(exec.phyCoreId);
    (*fields)["blockType"] = FormatBlockType(exec.blockType);
    (*fields)["blockId"] = std::to_string(exec.blockId);
    (*fields)["pipeName"] = OrUnknown(exec.pipeName);
    (*fields)["launchId"] = std::to_string(exec.launchId);
    (*fields)["binaryId"] = std::to_string(exec.binaryId);
    (*fields)["functionId"] = std::to_string(exec.functionId);
    (*fields)["location"] = FormatLocation(exec, true);
}

void PutPrefixedExecFields(const NpusanReportExecContext& exec, const std::string& prefix, ReportFields* fields)
{
    (*fields)[prefix + "CoreId"] = FormatCoreId(exec.phyCoreId);
    (*fields)[prefix + "Type"] = FormatBlockType(exec.blockType);
    (*fields)[prefix + "Block"] = std::to_string(exec.blockId);
    (*fields)[prefix + "Pipe"] = OrUnknown(exec.pipeName);
    (*fields)[prefix + "Pc"] = Hex(exec.pc);
    (*fields)[prefix + "KernelName"] = OrUnknown(exec.kernelName);
    (*fields)[prefix + "Function"] = OrUnknown(exec.function);
    (*fields)[prefix + "Offset"] = Hex(exec.offset);
    (*fields)[prefix + "File"] = OrUnknown(exec.file);
    (*fields)[prefix + "Line"] = std::to_string(exec.line);
    (*fields)[prefix + "Location"] = FormatLocation(exec, false);
}

void PutAccessFields(const NpusanReportMemoryAccess& access, ReportFields* fields)
{
    (*fields)["space"] = MemorySpaceName(access.memorySpace);
    (*fields)["access"] = AccessModeName(access.accessMode);
    (*fields)["accessBytes"] = std::to_string(access.accessBytes);
    (*fields)["requiredAlign"] = std::to_string(access.requiredAlign);
    (*fields)["address"] = Hex(access.address);
}

void PutAllocationFields(const NpusanReportAllocation& allocation, ReportFields* fields)
{
    (*fields)["allocId"] = std::to_string(allocation.allocId);
    (*fields)["base"] = Hex(allocation.base);
    (*fields)["bytes"] = std::to_string(allocation.bytes);
    (*fields)["allocSerialNo"] = std::to_string(allocation.allocSerialNo);
    (*fields)["freeSerialNo"] = std::to_string(allocation.freeSerialNo);
}

void PutDefaultHostFields(ReportFields* fields)
{
    (*fields)["hostFunction"] = "<unknown>";
    (*fields)["hostPc"] = "0";
    (*fields)["hostBinary"] = "<unknown>";
    (*fields)["allocFunction"] = "<unknown>";
    (*fields)["allocPc"] = "0";
    (*fields)["freeFunction"] = "<unknown>";
    (*fields)["freePc"] = "0";
}

std::vector<ReportCallStack> ActiveCallStacks(const NpusanReportCommon& common)
{
    return std::vector<ReportCallStack>(common.stacks.begin(), common.stacks.begin() + common.stackCount);
}

const ReportCallStack* FindStackByRole(const NpusanReportCommon& common, ReportStackRole role)
{
    for (std::uint32_t i = 0; i < common.stackCount && i < common.stacks.size(); ++i) {
        if (common.stacks[i].role == role) {
            return &common.stacks[i];
        }
    }
    return nullptr;
}

const ReportFrame* FirstStructuredFrame(const NpusanReportCommon& common, ReportStackRole role)
{
    const ReportCallStack* stack = FindStackByRole(common, role);
    if (stack == nullptr || (stack->format != ReportStackFormat::FRAMES && stack->format != ReportStackFormat::BOTH)) {
        return nullptr;
    }
    for (const ReportFrame& frame : stack->frames) {
        if (!frame.function.empty() || !frame.file.empty()) {
            return &frame;
        }
    }
    return nullptr;
}

void PutFrameLocationFields(const ReportFrame& frame, const std::string& prefix, ReportFields* fields)
{
    (*fields)[prefix + "Function"] = OrUnknown(frame.function);
    (*fields)[prefix + "Offset"] = Hex(frame.offset);
    (*fields)[prefix + "File"] = OrUnknown(frame.file);
    (*fields)[prefix + "Line"] = std::to_string(frame.line);
    (*fields)[prefix + "Location"] = FormatLocation(frame, false);
}

void PutFaultLocationFields(const NpusanReportCommon& common, ReportFields* fields)
{
    const ReportFrame* frame = FirstStructuredFrame(common, ReportStackRole::FAULT_DEVICE);
    if (frame == nullptr) {
        return;
    }
    (*fields)["function"] = OrUnknown(frame->function);
    (*fields)["offset"] = Hex(frame->offset);
    (*fields)["file"] = OrUnknown(frame->file);
    (*fields)["line"] = std::to_string(frame->line);
    (*fields)["location"] = FormatLocation(*frame, true);
}

} // namespace aclsan::cann::detail
