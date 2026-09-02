// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi/probe_source_generator.h"
#include "dbi/embedded_probe_resources.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aclsan {
namespace {

#define CHECK(condition)                                                                 \
    do {                                                                                 \
        if (!(condition)) {                                                              \
            std::cerr << "check failed at line " << __LINE__ << ": " #condition << '\n'; \
            return false;                                                                \
        }                                                                                \
    } while (false)

bool GeneratesCompleteDeterministicGroupSources()
{
    const std::vector<std::pair<ProbeGroup, std::size_t>> expected{
        {ProbeGroup::Mte1, 23U},   {ProbeGroup::Mte2, 19U},  {ProbeGroup::Mte3, 2U},
        {ProbeGroup::Fixpipe, 7U}, {ProbeGroup::Scalar, 8U}, {ProbeGroup::Sync, 24U},
    };
    for (const auto& [group, symbolCount] : expected) {
        const GeneratedProbeSource first = GenerateProbeSource("dav-3510", group);
        const GeneratedProbeSource second = GenerateProbeSource("dav-3510", group);
        CHECK(first.success);
        CHECK(first.diagnostic.empty());
        CHECK(first.symbols.size() == symbolCount);
        CHECK(!first.source.empty());
        CHECK(!first.sourceMap.empty());
        CHECK(first.identity.size() == 16U);
        CHECK(first.source == second.source);
        CHECK(first.sourceMap == second.sourceMap);
        CHECK(first.identity == second.identity);
    }
    return true;
}

bool RendersControlledMte2Definition()
{
    const GeneratedProbeSource generated = GenerateProbeSource("dav-3510", ProbeGroup::Mte2);
    CHECK(generated.success);
    CHECK(generated.source.find("#include \"trace_record.h\"") != std::string::npos);
    CHECK(generated.source.find("__sanitizer_report_copy_gm_to_cbuf_align_v2_b8") != std::string::npos);
    CHECK(generated.source.find("static_cast<uint16_t>(PIPE_MTE2), 74") != std::string::npos);
    CHECK(generated.source.find("// probe-definition: 0074") != std::string::npos);
    CHECK(generated.sourceMap.find("apiId=74") != std::string::npos);
    return true;
}

std::string_view FindProbeDefinition(std::string_view source, std::string_view symbol)
{
    const std::size_t symbolPosition = source.find(symbol);
    if (symbolPosition == std::string_view::npos) {
        return {};
    }
    const std::size_t definitionPosition = source.rfind("// probe-definition:", symbolPosition);
    if (definitionPosition == std::string_view::npos) {
        return {};
    }
    const std::size_t nextDefinition = source.find("// probe-definition:", symbolPosition);
    return source.substr(definitionPosition, nextDefinition - definitionPosition);
}

bool RendersNormalizedVectorSyncDefinitions()
{
    const GeneratedProbeSource generated = GenerateProbeSource("dav-3510", ProbeGroup::Sync);
    CHECK(generated.success);

    const std::vector<std::pair<std::string_view, std::string_view>> expected{
        {"__sanitizer_report_set_flag_v",
         "static_cast<uint64_t>(PIPE_V), static_cast<uint64_t>(dstPipe), eventId, 0UL, 0UL"},
        {"__sanitizer_report_set_flagi_v",
         "static_cast<uint64_t>(PIPE_V), static_cast<uint64_t>(dstPipe), eventId, 0UL, 0UL"},
        {"__sanitizer_report_wait_flag_v",
         "static_cast<uint64_t>(srcPipe), static_cast<uint64_t>(PIPE_V), eventId, 0UL, 0UL"},
        {"__sanitizer_report_wait_flagi_v",
         "static_cast<uint64_t>(srcPipe), static_cast<uint64_t>(PIPE_V), eventId, 0UL, 0UL"},
        {"__sanitizer_report_get_buf_v",
         "static_cast<uint64_t>(PIPE_V), static_cast<uint64_t>(bufId), static_cast<uint64_t>(mode), 0UL, 0UL"},
        {"__sanitizer_report_get_bufi_v",
         "static_cast<uint64_t>(PIPE_V), bufId, static_cast<uint64_t>(mode), 0UL, 0UL"},
        {"__sanitizer_report_rls_buf_v",
         "static_cast<uint64_t>(PIPE_V), static_cast<uint64_t>(bufId), static_cast<uint64_t>(mode), 0UL, 0UL"},
        {"__sanitizer_report_rls_bufi_v",
         "static_cast<uint64_t>(PIPE_V), bufId, static_cast<uint64_t>(mode), 0UL, 0UL"},
    };
    for (const auto& [symbol, arguments] : expected) {
        const std::string_view definition = FindProbeDefinition(generated.source, symbol);
        CHECK(!definition.empty());
        CHECK(definition.find(arguments) != std::string_view::npos);
    }
    return true;
}

bool RejectsUnsupportedRequests()
{
    CHECK(!GenerateProbeSource("dav-unknown", ProbeGroup::Mte2).success);
    CHECK(!GenerateProbeSource("dav-3510", static_cast<ProbeGroup>(255)).success);
    CHECK(ProbeGeneratorIdentity().size() == 16U);
    return true;
}

bool EmbedsPrivateProbeHeaders()
{
    CHECK(!EmbeddedTraceRecordHeader().empty());
    CHECK(!EmbeddedTraceBufferAbiHeader().empty());
    CHECK(EmbeddedTraceRecordHeader().find("WriteTraceRecord") != std::string_view::npos);
    CHECK(EmbeddedTraceBufferAbiHeader().find("AclsanRawTraceRecord") != std::string_view::npos);
    CHECK(EmbeddedProbeResourceIdentity().size() == 64U);
    CHECK(EmbeddedCtrlBinImplementationIdentity().size() == 64U);
    return true;
}

} // namespace
} // namespace aclsan

int main()
{
    return aclsan::GeneratesCompleteDeterministicGroupSources() && aclsan::RendersControlledMte2Definition() &&
                   aclsan::RendersNormalizedVectorSyncDefinitions() && aclsan::RejectsUnsupportedRequests() &&
                   aclsan::EmbedsPrivateProbeHeaders() ?
               0 :
               1;
}
