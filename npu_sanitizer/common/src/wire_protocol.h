/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_CHECK_WIRE_PROTOCOL_H
#define NPU_CHECK_WIRE_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace npu::sanitizer::ipc {

constexpr uint32_t kProtocolMagic = 0x4e53414eu;
constexpr uint16_t kProtocolMajor = 1;
constexpr uint16_t kProtocolMinor = 0;
// 帧头固定 36 字节，布局见下方 EncodeFrame。字段顺序、宽度与偏移属于 wire ABI，
// 增删字段或改变偏移都是 major 变更。
constexpr size_t kWireHeaderSize = 36;
constexpr size_t kMaxPayloadSize = 64 * 1024;
constexpr size_t kMaxFrameSize = kWireHeaderSize + kMaxPayloadSize; // 65572
constexpr size_t kMaxCompileOptions = 128;

// 帧头 flags 的位定义。
//
// bit 0-2 是 must-understand 的语义位，bit 3-7 保留且必须为 0（接收端发现非零即协议
// 错误），bit 8-15 保留给未来 minor 新增的 must-ignore 标志（接收端必须忽略无法识别
// 的置位）。按位区分这两类，使得 minor 版本可以在不破坏旧实现的前提下新增标志。
constexpr uint16_t kFlagMore = 1u << 0;      // 仅 Result 有效：同一报告还有续帧
constexpr uint16_t kFlagHasErrors = 1u << 1; // 仅 Result 末帧有效：本次检查发现了问题
constexpr uint16_t kFlagTruncated = 1u << 2; // 仅 Result 末帧有效：报告因触及总长上限被截断
constexpr uint16_t kFlagsReservedMustUnderstand = 0x00f8u; // bit 3-7
constexpr uint16_t kFlagsReservedMustIgnore = 0xff00u;     // bit 8-15

// UDS 地址走 Linux 抽象命名空间，不在文件系统里创建 socket 文件，因此这里是"名字"
// 而不是"路径"。取值形如 @npu_check-<32 位十六进制随机数>：前导 '@' 只是便于人读，
// 也与 ss -x、/proc/net/unix 的显示一致，两端填充 sockaddr_un 时必须把它换成 '\0'。
// 具体填充与 addrLen 计算见 uds_transport.h 的 BuildAbstractAddress。
constexpr const char* kUdsNameEnv = "NPU_CHECK_UDS_NAME";
constexpr const char* kSessionIdEnv = "NPU_CHECK_SESSION_ID";
constexpr const char* kCliPidEnv = "NPU_CHECK_CLI_PID";
constexpr const char* kHandshakeTimeoutEnv = "NPU_CHECK_HANDSHAKE_TIMEOUT_MS";

// 消息类型。
//
// 最高位（0x8000）表示该消息是否可选：置 0 为 must-understand，收到未知类型即协议
// 错误；置 1 为 must-ignore，收到未知类型必须跳过并继续。这条规则让 minor 版本可以
// 新增消息而不破坏旧实现。
enum class MessageType : uint16_t {
    CLIENT_HELLO = 0x0001,
    SERVER_HELLO = 0x0002,
    CONFIGURE = 0x0003,
    READY = 0x0004,
    DIAGNOSTIC = 5,
    LOG = 6,
    SUMMARY = 7,
    SESSION_END = 8,
    ERROR = 0x00ff,

    // must-ignore 区间。实时诊断只供人阅读，不参与退出码判定；权威结论由 Result 承载。
    DIAGNOSTIC_STREAM = 0x8001,
};

// 未知类型是否可以安全跳过。
constexpr bool IsMustIgnoreType(uint16_t type) { return (type & 0x8000u) != 0; }

struct Frame {
    MessageType type = MessageType::ERROR;
    uint16_t flags = 0;
    uint64_t sessionId = 0;
    uint64_t sequence = 0;
    std::vector<uint8_t> payload;
};

// Hello payload 固定 8 字节。这两个字段的唯一用途是与 SO_PEERCRED 的结果交叉比对；
// 会话归属由帧头的 session_id 承担，payload 不再携带任何会话秘密。
//
// 原先这里有一个 nonce，其职责"防止连到其它并发会话"已被两层挡死：抽象地址本身是
// getrandom 生成的唯一随机名，双方都只认自己环境变量里的那一个；SO_PEERCRED 又在两侧
// 把对端 pid 钉死，且来自内核无法伪造。而 session_id 是每帧校验，比 nonce 只在 Hello
// 校验一次覆盖得更早更全，因此 nonce 能挡的场景 session_id 全能挡，反之不成立。
struct HelloPayload {
    uint32_t pid = 0;
    uint32_t uid = 0;
};

struct ToolConfig {
    std::string toolName;
    bool strict = true;
    bool keepTemp = true;
    std::string logFile;
    std::string workDir;
    std::string probeCacheDir;
    std::vector<std::string> compileOptions;
};

// ---------------------------------------------------------------------------
// 共享协议定义与选项注册表
//
// 本节是 ToolId / OptionId / OptionValue / ToolRequest 的唯一定义处。CLI 与注入库
// 链接同一 common 目标，两端禁止各自复制一份 ID 或编码表。
//
// 注意：当前 Configure 线路格式仍是上面字符串式的 ToolConfig。下列类型先行落地，
// 供 CLI 的参数解析使用；Configure 编码迁移到本注册表属于后续改动。
// ---------------------------------------------------------------------------

enum class ToolId : uint16_t {
    kMemcheck = 1,  // 内存访问与生命周期检查
    kSynccheck = 2, // 同步原语及 barrier 使用检查
};

enum class OptionId : uint16_t {
    kMemcheckCheckCacheControl = 0x0101,
    kSynccheckMissingBarrierInitIsFatal = 0x0201,
};

// 以下为内存逻辑模型，与线路布局无关：字段顺序、对齐、padding 都可能与线上不同，
// 编码器必须逐字段读写，不得把结构体整体 memcpy 到帧里。
struct OptionValue {
    OptionId optionId = OptionId::kMemcheckCheckCacheControl; // 唯一确定所属工具和值规则
    std::vector<uint8_t> value;                               // 注册表编码器生成的规范字节
};

struct ToolRequest {
    ToolId toolId = ToolId::kMemcheck;
    std::vector<OptionValue> options; // 按 optionId 升序，不重复
};

// 工具子选项的注册项。名称在 CLI 中全局唯一，因此子选项可以出现在所属 --tool 之前或
// 之后，解析器不依赖"当前工具"状态，也不通过参数相邻关系推断归属。
struct OptionRegistryEntry {
    const char* name;         // 命令行名称，不含前导 "--"
    OptionId optionId;        // 唯一确定所属工具、值编码与校验规则
    ToolId toolId;            // 所属工具
    uint16_t introducedMinor; // 引入该选项的协议 minor
    uint16_t valueSize;       // V1 均为 1
    uint8_t presentValue;     // "出现即为真"的开关值，V1 均为 0x01
};

// 工具名与 ToolId 的双向查找。未知工具名必须被拒绝，不能静默忽略。
const char* ToolName(ToolId id);
bool LookupTool(const std::string& name, ToolId& id);

// 按命令行名称（不含 "--"）查注册项；未注册时返回 nullptr。
const OptionRegistryEntry* LookupOption(const std::string& name);

// 全量注册表，供帮助文本和测试遍历。
const std::vector<OptionRegistryEntry>& OptionRegistry();

std::vector<uint8_t> EncodeFrame(const Frame& frame);
bool DecodeFrame(const uint8_t* data, size_t size, Frame& frame, std::string& error);

std::vector<uint8_t> EncodeHello(const HelloPayload& hello);
bool DecodeHello(const std::vector<uint8_t>& payload, HelloPayload& hello, std::string& error);

std::vector<uint8_t> EncodeToolConfig(const ToolConfig& config);
bool DecodeToolConfig(const std::vector<uint8_t>& payload, ToolConfig& config, std::string& error);

std::vector<uint8_t> EncodeText(const std::string& text);
bool DecodeText(const std::vector<uint8_t>& payload, std::string& text, std::string& error);

const char* MessageTypeName(MessageType type);

} // namespace npu::sanitizer::ipc

#endif
