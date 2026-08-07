#include "cann_sanitizer_context.h"

namespace ascsan::cann {

ParsedInstruction ParseInstruction(const AscsanDeviceInstructionData &instruction)
{
    ParsedInstruction parsed{};
    parsed.pipeline = static_cast<AscsanPatchPipeline>(instruction.pipeline);
    parsed.cbid = instruction.cbid;
    parsed.siteId = instruction.siteId;
    parsed.pc = instruction.pc;

    switch (parsed.pipeline) {
        case ASCSAN_PATCH_PIPELINE_MTE2:
            parsed.kind = ParsedInstruction::Kind::MemoryTransfer;
            parsed.op = "MTE2";
            parsed.src = instruction.rawArgs[0];
            parsed.dst = instruction.rawArgs[1];
            parsed.bytes = instruction.rawArgs[2];
            break;
        case ASCSAN_PATCH_PIPELINE_MTE3:
            parsed.kind = ParsedInstruction::Kind::MemoryTransfer;
            parsed.op = "MTE3";
            parsed.src = instruction.rawArgs[0];
            parsed.dst = instruction.rawArgs[1];
            parsed.bytes = instruction.rawArgs[2];
            break;
        case ASCSAN_PATCH_PIPELINE_FIXPIPE:
            parsed.kind = ParsedInstruction::Kind::Fixpipe;
            parsed.op = "FIXPIPE";
            parsed.src = instruction.rawArgs[0];
            parsed.dst = instruction.rawArgs[1];
            parsed.bytes = instruction.rawArgs[2];
            parsed.aux0 = instruction.rawArgs[3];
            parsed.aux1 = instruction.rawArgs[4];
            break;
        case ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG:
            parsed.kind = ParsedInstruction::Kind::SyncFlag;
            parsed.op = "SET_WAIT_FLAG";
            parsed.aux0 = instruction.rawArgs[0];
            parsed.aux1 = instruction.rawArgs[1];
            break;
        case ASCSAN_PATCH_PIPELINE_GET_RLS_BUF:
            parsed.kind = ParsedInstruction::Kind::BufferLifetime;
            parsed.op = "GET_RLS_BUF";
            parsed.src = instruction.rawArgs[0];
            parsed.bytes = instruction.rawArgs[1];
            parsed.aux0 = instruction.rawArgs[2];
            break;
        default:
            parsed.kind = ParsedInstruction::Kind::Unknown;
            parsed.op = "UNKNOWN";
            break;
    }
    return parsed;
}

} // namespace ascsan::cann

