// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "tool_manager/tool_manager.h"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>

namespace npu::sanitizer {
namespace {

std::string StatusReason(AclsanStatus status) { return "api_status_" + std::to_string(static_cast<uint32_t>(status)); }

bool HasCallStackFrames(AclsanStatus status)
{
    return status == ACLSAN_STATUS_SUCCESS || status == ACLSAN_STATUS_ERROR_MAX_LIMIT_REACHED;
}

std::string FormatCallStackReport(AclsanStatus status, const AclsanDeviceCallStack& callStack)
{
    std::ostringstream output;
    output << "[CALL-STACK] pc=0x" << std::hex << callStack.pc << std::dec;
    if (!HasCallStackFrames(status) || callStack.depth == 0) {
        output << " status=unavailable reason=" << StatusReason(status) << '\n';
        return output.str();
    }
    output << " status=available binary_id=" << callStack.binaryId;
    if ((callStack.flags & ACLSAN_CALL_STACK_FLAG_TRUNCATED) != 0) {
        output << " truncated=true";
    }
    output << '\n';
    for (uint32_t index = 0; index < callStack.depth; ++index) {
        const AclsanDeviceCallStackFrame& frame = callStack.frames[index];
        output << "  #" << index << ' ' << frame.functionName << " at " << frame.fileName << ':' << frame.line << ':'
               << frame.column << '\n';
    }
    return output.str();
}

std::vector<aclsan::cann::ReportFrame> MakeReportFrames(const AclsanDeviceCallStack& callStack)
{
    const uint32_t depth = std::min(callStack.depth, static_cast<uint32_t>(ACLSAN_CALL_STACK_MAX_DEPTH));
    std::vector<aclsan::cann::ReportFrame> frames;
    frames.reserve(depth);
    for (uint32_t index = 0; index < depth; ++index) {
        const AclsanDeviceCallStackFrame& source = callStack.frames[index];
        aclsan::cann::ReportFrame frame;
        frame.pc = callStack.pc;
        frame.function = source.functionName;
        frame.file = source.fileName;
        frame.line = source.line;
        frame.column = source.column;
        frame.inlineDepth = source.inlineDepth;
        frames.push_back(std::move(frame));
    }
    return frames;
}

void PopulateDeviceCallStack(aclsan::cann::NpusanMemcheckReport& report) noexcept
{
    if (report.common.exec.pc == 0 || report.common.stackCount > aclsan::cann::kNpusanReportStackMax) {
        return;
    }

    std::uint32_t stackIndex = report.common.stackCount;
    for (std::uint32_t index = 0; index < report.common.stackCount && index < aclsan::cann::kNpusanReportStackMax;
         ++index) {
        if (report.common.stacks[index].role == aclsan::cann::ReportStackRole::kFaultDevice) {
            stackIndex = index;
            break;
        }
    }

    if (stackIndex == report.common.stackCount && report.common.stackCount >= aclsan::cann::kNpusanReportStackMax) {
        return;
    }

    try {
        auto callStack = std::make_unique<AclsanDeviceCallStack>();
        const AclsanStatus status = aclsanGetDeviceCallStack(report.common.exec.pc, callStack.get());
        std::string callStackText = FormatCallStackReport(status, *callStack);
        std::vector<aclsan::cann::ReportFrame> frames;
        if (HasCallStackFrames(status)) {
            frames = MakeReportFrames(*callStack);
        }

        auto& stack = report.common.stacks[stackIndex];
        stack.rawText.swap(callStackText);
        stack.role = aclsan::cann::ReportStackRole::kFaultDevice;
        if (!frames.empty()) {
            stack.frames.swap(frames);
        }
        const bool hasStructuredFrames = !stack.frames.empty();
        stack.format =
            hasStructuredFrames ? aclsan::cann::ReportStackFormat::kBoth : aclsan::cann::ReportStackFormat::kRawText;
        if (callStack->binaryId != 0) {
            report.common.exec.binaryId = callStack->binaryId;
        }
        if (stackIndex == report.common.stackCount) {
            ++report.common.stackCount;
        }
    } catch (...) {
        return;
    }
}

void PopulateSyncPointCallStack(
    aclsan::cann::NpusanReportCommon& common, aclsan::cann::NpusanSyncPoint& point,
    aclsan::cann::ReportStackRole role) noexcept
{
    if (!point.hasExecContext || point.exec.pc == 0 || common.stackCount > aclsan::cann::kNpusanReportStackMax) {
        return;
    }

    std::uint32_t stackIndex = common.stackCount;
    for (std::uint32_t index = 0; index < common.stackCount && index < aclsan::cann::kNpusanReportStackMax; ++index) {
        if (common.stacks[index].role == role) {
            stackIndex = index;
            break;
        }
    }
    if (stackIndex == common.stackCount && common.stackCount >= aclsan::cann::kNpusanReportStackMax) {
        return;
    }

    try {
        auto callStack = std::make_unique<AclsanDeviceCallStack>();
        const AclsanStatus status = aclsanGetDeviceCallStack(point.exec.pc, callStack.get());
        std::string callStackText = FormatCallStackReport(status, *callStack);
        std::vector<aclsan::cann::ReportFrame> frames;
        if (HasCallStackFrames(status)) {
            frames = MakeReportFrames(*callStack);
        }

        auto& stack = common.stacks[stackIndex];
        stack.rawText.swap(callStackText);
        stack.role = role;
        stack.frames.swap(frames);
        stack.format =
            stack.frames.empty() ? aclsan::cann::ReportStackFormat::kRawText : aclsan::cann::ReportStackFormat::kBoth;
        if (callStack->binaryId != 0) {
            point.exec.binaryId = callStack->binaryId;
        }
        point.stackRole = role;
        if (stackIndex == common.stackCount) {
            ++common.stackCount;
        }
    } catch (...) {
        return;
    }
}

void PopulateDeviceCallStack(aclsan::cann::NpusanSynccheckReport& report) noexcept
{
    PopulateSyncPointCallStack(report.common, report.triggerPoint, aclsan::cann::ReportStackRole::kSyncTrigger);
    report.common.exec = report.triggerPoint.exec;
    PopulateSyncPointCallStack(report.common, report.relatedPoint, aclsan::cann::ReportStackRole::kSyncRelated);
}

struct CallbackSpec {
    AclsanCallbackDomain domain;
    AclsanCallbackId cbid;
};

constexpr std::array<CallbackSpec, 4> kMemcheckCallbacks{{
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC},
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_FREE},
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
}};

constexpr std::array<CallbackSpec, 2> kSynccheckCallbacks{{
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
}};

std::string StatusMessage(const char* operation, AclsanStatus status)
{
    std::ostringstream output;
    output << operation << " failed with AclsanStatus=" << static_cast<int>(status);
    return output.str();
}

} // namespace

ToolManager::~ToolManager() noexcept
{
    try {
        Finalize();
    } catch (...) {
        // Destruction runs at process exit and must not cross the C injection boundary.
        return;
    }
}

ToolManager::ActiveCallbackGuard::~ActiveCallbackGuard() noexcept { service_.LeaveCallback(); }

int ToolManager::Initialize()
{
    std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    if (initialized_) {
        return 0;
    }

    std::string error;
    if (!server_.StartAndHandshake(configure_, error)) {
        LogHandshakeFailure(error);
        server_.Shutdown();
        return 1;
    }
    if (!InitializeLogger(error)) {
        server_.SendInitializationError(ipc::ErrorDomain::kInjection, ipc::error_code::kLoggerOpenFailed, error);
        server_.Shutdown();
        return 1;
    }
    std::ostringstream handshakeMessage;
    handshakeMessage << "UDS handshake completed session=" << server_.SessionId()
                     << " negotiated_minor=" << server_.NegotiatedMinor();
    logger_.Info(handshakeMessage.str());
    std::ostringstream configMessage;
    configMessage << "tool configuration work_dir=" << workDir_ << " tool_count=" << configure_.tools.size();
    for (const auto& tool : configure_.tools) {
        configMessage << " tool=" << ipc::ToolName(tool.toolId) << " option_count=" << tool.options.size();
        for (const auto& option : tool.options) {
            configMessage << " option_id=0x" << std::hex << static_cast<unsigned>(option.optionId) << std::dec;
        }
    }
    logger_.Info(configMessage.str());
    if (!ConfigureSanitizer(error)) {
        logger_.Error(error);
        server_.SendInitializationError(
            ipc::ErrorDomain::kConfiguration, ipc::error_code::kToolInitializationFailed, error);
        RollbackSanitizer();
        server_.Shutdown();
        return 1;
    }
    // Ready 不带 payload，会话细节只写本地日志。
    logger_.Info(BuildReadyMessage());
    if (!server_.SendReady(error)) {
        logger_.Error(error);
        RollbackSanitizer();
        server_.Shutdown();
        return 1;
    }
    // 这里曾经把 logger 的错误接到 UDS 的 Error 帧上。新协议里 Error 表示"基础设施失败、
    // 本次检查结论不可用"，CLI 收到即退 125；一次日志写盘失败显然够不上这个级别，却会
    // 让整次检查作废。日志错误只留在本地 npu_check.log 里，不再上线路。
    initialized_ = true;
    logger_.Info("npu_check initialization completed");
    return 0;
}

void ToolManager::LogHandshakeFailure(const std::string& reason) noexcept
{
    try {
        std::string ignored;
        const std::filesystem::path path = std::filesystem::current_path() / "npu_check.log";
        if (logger_.Path().empty() && !logger_.Open(path.string(), logging::Logger::ConfiguredLevel(), ignored)) {
            return;
        }
        logger_.Error("UDS handshake failed: " + reason);
        logger_.Flush();
    } catch (...) {
        return;
    }
}

bool ToolManager::InitializeLogger(std::string& error)
{
    // 工作目录经环境变量传入。Configure 改用注册表编码后只承载工具与子选项，路径这类
    // 与协议无关的部署信息不再占线路。未设置时退回当前目录。
    const char* workDir = std::getenv(ipc::kWorkDirEnv);
    workDir_ = workDir != nullptr ? workDir : "";
    const std::filesystem::path directory =
        workDir_.empty() ? std::filesystem::current_path() : std::filesystem::path(workDir_);
    const std::string path = (directory / "npu_check.log").string();
    if (!logger_.Open(path, logging::Logger::ConfiguredLevel(), error)) {
        return false;
    }
    return true;
}

bool ToolManager::IsToolEnabled(ipc::ToolId toolId) const
{
    for (const auto& tool : configure_.tools) {
        if (tool.toolId == toolId) {
            return true;
        }
    }
    return false;
}

bool ToolManager::ConfigureSanitizer(std::string& error)
{
    if (configure_.tools.empty()) {
        error = "configure enabled no tool";
        return false;
    }
    // 按 toolId 升序逐个构造 checker。工具之间没有互斥关系，memcheck 与 synccheck
    // 可以在同一次运行中同时启用；任一构造失败即整体失败，不发 Ready。
    for (const auto& tool : configure_.tools) {
        switch (tool.toolId) {
            case ipc::ToolId::kMemcheck:
                memcheck_ = std::make_unique<Memcheck>(true);
                logger_.Debug("memcheck instance created");
                break;
            case ipc::ToolId::kSynccheck:
                synccheck_ = std::make_unique<Synccheck>(&logger_);
                logger_.Debug("synccheck instance created");
                break;
            default:
                error = std::string("unsupported tool '") + ipc::ToolName(tool.toolId) + "'";
                return false;
        }
    }
    AclsanStatus status = aclsanSubscribe(&subscriber_, &ToolManager::Callback, this);
    if (status != ACLSAN_STATUS_SUCCESS) {
        error = StatusMessage("aclsanSubscribe", status);
        return false;
    }
    subscribed_ = true;
    logger_.Info("sanitizer callback subscriber registered");
    return EnableCallbacks(error);
}

bool ToolManager::EnableCallbacks(std::string& error)
{
    // 需要使能的回调是各 checker 声明集合的并集，去重后一次性使能。
    //
    // 去重不是优化：SYNCHRONIZE/STREAM_SYNC_END 被 memcheck 与 synccheck 共用，两个
    // 工具同时启用时若各使能一次，同一事件会被投递两次，配对与统计逻辑都会出错。
    std::vector<CallbackSpec> required;
    const auto collect = [&required](const auto& callbacks) {
        for (const auto& callback : callbacks) {
            const bool duplicate = std::any_of(required.begin(), required.end(), [&callback](const CallbackSpec& spec) {
                return spec.domain == callback.domain && spec.cbid == callback.cbid;
            });
            if (!duplicate) {
                required.push_back(callback);
            }
        }
    };
    if (memcheck_ != nullptr) {
        collect(kMemcheckCallbacks);
    }
    if (synccheck_ != nullptr) {
        collect(kSynccheckCallbacks);
    }

    for (const auto& callback : required) {
        const AclsanStatus status = aclsanEnableCallback(1, subscriber_, callback.domain, callback.cbid);
        if (status != ACLSAN_STATUS_SUCCESS) {
            error = StatusMessage("aclsanEnableCallback", status);
            return false;
        }
        std::ostringstream message;
        message << "callback enabled domain=" << static_cast<uint32_t>(callback.domain)
                << " cbid=" << static_cast<uint32_t>(callback.cbid);
        logger_.Debug(message.str());
    }
    return true;
}

void ToolManager::RollbackSanitizer()
{
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        stopping_ = true;
    }
    if (subscribed_) {
        (void)aclsanUnsubscribe(subscriber_);
        subscribed_ = false;
        subscriber_ = nullptr;
    }
    {
        std::unique_lock<std::mutex> callbackLock(callbackMutex_);
        callbacksDrained_.wait(callbackLock, [this] { return activeCallbacks_ == 0; });
    }
    memcheck_.reset();
    synccheck_.reset();
}

void ToolManager::Finalize()
{
    std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    if (!initialized_ && !subscribed_) {
        return;
    }
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        stopping_ = true;
    }
    AclsanStatus unsubscribeStatus = ACLSAN_STATUS_SUCCESS;
    if (subscribed_) {
        unsubscribeStatus = aclsanUnsubscribe(subscriber_);
        subscribed_ = false;
        subscriber_ = nullptr;
    }
    {
        std::unique_lock<std::mutex> callbackLock(callbackMutex_);
        callbacksDrained_.wait(callbackLock, [this] { return activeCallbacks_ == 0; });
    }

    // 先把实时诊断队列排空并停掉 publisher 线程：一是让 Result 独占后续线路，二是
    // dropped_messages 只有等队列落定之后才是终值，否则写进报告的是中间数。
    server_.StopPublisher();

    const std::string summary = BuildSummaryMessage();
    logger_.Info(summary);
    // 多工具时取"全部工具都分析完整"，任一工具留有在途或被丢弃的事件，整份报告就
    // 不能声称完整 —— 这里必须是与，不是二选一。
    bool analysisComplete = true;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        if (memcheck_ != nullptr) {
            const MemcheckStats stats = memcheck_->Stats();
            analysisComplete =
                analysisComplete && stats.pendingDeviceOperations == 0 && stats.droppedDeviceOperations == 0;
        }
        if (synccheck_ != nullptr) {
            const SynccheckStats stats = synccheck_->Stats();
            analysisComplete = analysisComplete && stats.pendingOpens == 0 && stats.invalidEvents == 0;
        }
        analysisComplete = analysisComplete && malformedCallbacks_ == 0 && frameworkErrors_ == 0;
    }
    const bool truncated = report_.Truncated();
    std::ostringstream sessionEnd;
    const bool transportComplete = server_.TransportComplete() && server_.DroppedMessages() == 0;
    sessionEnd << "status="
               << (unsubscribeStatus == ACLSAN_STATUS_SUCCESS && transportComplete && analysisComplete ? "complete" :
                                                                                                         "incomplete")
               << " aclsan_unsubscribe=" << static_cast<int>(unsubscribeStatus)
               << " dropped_messages=" << server_.DroppedMessages()
               << " analysis_complete=" << (analysisComplete ? "true" : "false")
               << " report_truncated=" << (truncated ? "true" : "false");

    // 汇总与完整性信息进报告正文，而不是单独的消息类型：CLI 不解析报告内容，能不能
    // 信任这份结论由 Result 末帧的标志位表达，正文只负责让人读懂发生了什么。
    std::ostringstream trailer;
    trailer << "===== npu_check summary =====\n" << summary << '\n' << sessionEnd.str() << '\n';
    (void)report_.Append(trailer.str());

    const bool hasErrors = HasDetectedErrors();
    if (report_.Failed()) {
        // 报告本身没能拼出来，此时宁可什么都不给，也不能把残缺的正文当成结论发出去。
        server_.SendError(
            ipc::ErrorDomain::kInternal, ipc::error_code::kReportUnavailable,
            "npu_check cannot produce the session report");
    } else {
        const std::string reportText = report_.Take();
        std::string sendError;
        if (!server_.SendResult(reportText, hasErrors, truncated, sendError)) {
            logger_.Error("failed to deliver the session report: " + sendError);
        }
    }
    server_.Shutdown();
    logger_.Info(sessionEnd.str());
    logger_.Flush();
    memcheck_.reset();
    synccheck_.reset();
    initialized_ = false;
}

bool ToolManager::HasDetectedErrors() const
{
    // 多工具时任一工具检出即为真。
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    if (memcheck_ != nullptr && memcheck_->Stats().errors != 0) {
        return true;
    }
    if (synccheck_ != nullptr) {
        const SynccheckStats stats = synccheck_->Stats();
        if (stats.duplicateOpens + stats.unmatchedCloses + stats.unconsumedOpens != 0) {
            return true;
        }
    }
    return false;
}

bool ToolManager::IsInitialized() const
{
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    return initialized_;
}

void ToolManager::Callback(
    void* userdata, AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata) noexcept
{
    auto* service = static_cast<ToolManager*>(userdata);
    if (service == nullptr) {
        return;
    }
    if (!service->EnterCallback()) {
        return;
    }
    ActiveCallbackGuard callbackGuard(*service);
    try {
        service->OnCallback(domain, cbid, cbdata);
    } catch (const std::exception& exception) {
        service->OnCallbackException(exception.what());
    } catch (...) {
        service->OnCallbackException("unknown exception");
    }
}

bool ToolManager::EnterCallback()
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (stopping_) {
        return false;
    }
    ++activeCallbacks_;
    return true;
}

void ToolManager::LeaveCallback()
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (activeCallbacks_ != 0) {
        --activeCallbacks_;
    }
    if (activeCallbacks_ == 0) {
        callbacksDrained_.notify_all();
    }
}

void ToolManager::OnCallback(AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata)
{
    LogCallback(domain, cbid, cbdata);
    std::vector<aclsan::cann::NpusanMemcheckReport> reports;
    std::vector<aclsan::cann::NpusanSynccheckReport> syncReports;
    bool malformed = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        switch (domain) {
            case ACLSAN_CB_DOMAIN_RESOURCE: {
                const auto* data = ValidateCallbackData<AclsanResourceData>(cbdata);
                malformed = data == nullptr;
                if (data != nullptr && memcheck_ != nullptr && cbid == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) {
                    memcheck_->OnAllocation(*data);
                    std::ostringstream message;
                    message << "memory alloc resource=" << data->resourceId << " device=" << data->deviceId
                            << " address=" << data->ptr << " bytes=" << data->bytes
                            << " result=" << data->common.result;
                    logger_.Info(message.str());
                } else if (data != nullptr && memcheck_ != nullptr && cbid == ACLSAN_CBID_RESOURCE_MEMORY_FREE) {
                    memcheck_->OnFree(*data);
                    std::ostringstream message;
                    message << "memory free resource=" << data->resourceId << " device=" << data->deviceId
                            << " address=" << data->ptr << " result=" << data->common.result;
                    logger_.Info(message.str());
                }
                break;
            }
            case ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION: {
                if (cbid == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
                    const auto* data = ValidateDeviceMemoryAccessData(cbdata);
                    if (data != nullptr && memcheck_ != nullptr) {
                        memcheck_->QueueDeviceMemoryAccess(*data);
                        std::ostringstream message;
                        message << "device memory access launch=" << data->header.launchId << " pc=0x" << std::hex
                                << data->header.pc << std::dec << " device=" << data->header.deviceId
                                << " core=" << data->header.phyCoreId << " address=0x" << std::hex << data->address
                                << std::dec << " access_mode=" << data->accessMode << " layout=" << data->layoutKind;
                        logger_.Debug(message.str());
                    } else {
                        malformed = true;
                    }
                } else if (cbid == ACLSAN_CBID_DEVICE_SYNC) {
                    const auto* data = static_cast<const AclsanDeviceSyncData*>(cbdata);
                    if (data != nullptr && synccheck_ != nullptr) {
                        syncReports = synccheck_->OnDeviceSync(*data);
                    } else {
                        malformed = true;
                    }
                }
                break;
            }
            case ACLSAN_CB_DOMAIN_SYNCHRONIZE: {
                const auto* data = ValidateCallbackData<AclsanSynchronizeData>(cbdata);
                malformed = data == nullptr;
                if (data != nullptr && memcheck_ != nullptr && data->common.result == 0) {
                    reports = memcheck_->OnSynchronization();
                    std::ostringstream message;
                    message << "synchronization completed reports=" << reports.size() << " stream=" << data->stream;
                    logger_.Info(message.str());
                }
                if (data != nullptr && synccheck_ != nullptr) {
                    syncReports = synccheck_->OnSynchronization();
                    std::ostringstream message;
                    message << "synchronization observed reports=" << syncReports.size() << " stream=" << data->stream
                            << " result=" << data->common.result;
                    logger_.Info(message.str());
                }
                if (data != nullptr && data->common.result != 0) {
                    std::ostringstream message;
                    message << "synchronization failed result=" << data->common.result << " stream=" << data->stream;
                    logger_.Warning(message.str());
                }
                break;
            }
            default:
                break;
        }
    }
    if (malformed) {
        {
            std::lock_guard<std::mutex> stateLock(stateMutex_);
            ++malformedCallbacks_;
        }
        PublishMalformed(domain, cbid, "null, truncated, or incompatible callback data");
    } else {
        PublishDiagnostics(std::move(reports));
        PublishSynccheckReports(std::move(syncReports));
    }
}

void ToolManager::OnCallbackException(const char* reason) noexcept
{
    try {
        {
            std::lock_guard<std::mutex> stateLock(stateMutex_);
            ++frameworkErrors_;
        }
        std::string message = "npu_check callback failed: ";
        message += reason != nullptr ? reason : "unspecified exception";
        logger_.Error(message);
    } catch (...) {
        // Error reporting is best effort inside a noexcept runtime callback.
        return;
    }
}

void ToolManager::PublishDiagnostics(std::vector<aclsan::cann::NpusanMemcheckReport> reports)
{
    for (auto& report : reports) {
        PopulateDeviceCallStack(report);
        std::string rendered;
        const auto status =
            aclsan::cann::RenderNpusanReportRecord(aclsan::cann::NpusanReportRecord::From(report), {}, &rendered);
        if (status != aclsan::cann::ReportRenderStatus::kSuccess) {
            std::ostringstream message;
            message << "report rendering failed report_id=" << report.common.reportId
                    << " status=" << static_cast<int>(status);
            {
                std::lock_guard<std::mutex> stateLock(stateMutex_);
                ++frameworkErrors_;
            }
            logger_.Error(message.str());
            continue;
        }
        logger_.Info("diagnostic report generated report_id=" + std::to_string(report.common.reportId));
        RecordDiagnostic(rendered, "diagnostic");
    }
}

void ToolManager::PublishSynccheckReports(std::vector<aclsan::cann::NpusanSynccheckReport> reports)
{
    for (auto& report : reports) {
        PopulateDeviceCallStack(report);
        std::string rendered;
        const auto status =
            aclsan::cann::RenderNpusanReportRecord(aclsan::cann::NpusanReportRecord::From(report), {}, &rendered);
        if (status != aclsan::cann::ReportRenderStatus::kSuccess) {
            std::ostringstream message;
            message << "synccheck report rendering failed report_id=" << report.common.reportId
                    << " status=" << static_cast<int>(status);
            {
                std::lock_guard<std::mutex> stateLock(stateMutex_);
                ++frameworkErrors_;
            }
            logger_.Error(message.str());
            continue;
        }
        logger_.Info("synccheck diagnostic report generated report_id=" + std::to_string(report.common.reportId));
        RecordDiagnostic(rendered, "synccheck diagnostic");
    }
}

void ToolManager::RecordDiagnostic(const std::string& rendered, const char* what)
{
    // 权威路径：聚合进本次会话的 Result。追加失败只说明报告到顶了，不影响已有内容，
    // 截断这件事由末帧的 kFlagTruncated 告诉 CLI。
    if (!report_.Append(rendered) && !report_.Truncated()) {
        logger_.Error(std::string("failed to record ") + what + " into the session report");
    }
    // 辅助路径：实时流。它是 must-ignore 消息，只为长任务下的人工观察服务，投递失败
    // 不影响结论 —— 同一条诊断已经在 Result 里了。
    if (!server_.Publish(ipc::MessageType::DIAGNOSTIC_STREAM, rendered)) {
        logger_.Warning(std::string("failed to queue ") + what + " for live streaming");
    }
}

void ToolManager::PublishMalformed(AclsanCallbackDomain domain, AclsanCallbackId cbid, const char* reason)
{
    std::ostringstream output;
    output << "[NPU-CHECK-MALFORMED-CALLBACK] domain=" << static_cast<uint32_t>(domain) << " cbid=" << cbid
           << " reason=" << reason;
    logger_.Error(output.str());
}

void ToolManager::LogCallback(AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata)
{
    const uint64_t count = ++callbackCount_;
    std::ostringstream message;
    message << "cbdata received count=" << count << " domain=" << static_cast<uint32_t>(domain)
            << " cbid=" << static_cast<uint32_t>(cbid) << " address=" << cbdata;
    logger_.Debug(message.str());
}

std::string ToolManager::BuildReadyMessage() const
{
    std::ostringstream output;
    output << "session=" << server_.SessionId() << " api_version=" << ACLSAN_API_VERSION << " tools=";
    for (size_t index = 0; index < configure_.tools.size(); ++index) {
        output << (index == 0 ? "" : ",") << ipc::ToolName(configure_.tools[index].toolId);
    }
    output << " work_dir=" << workDir_;
    return output.str();
}

std::string ToolManager::BuildSummaryMessage() const
{
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    // 每个启用的工具一行，按 toolId 升序。多工具时不能再用 if/else 二选一 ——
    // 那样第二个工具的统计会整段消失，而报告看上去仍然完整。
    std::ostringstream output;
    if (memcheck_ != nullptr) {
        const MemcheckStats stats = memcheck_->Stats();
        output << "tool=memcheck allocations=" << stats.allocations << " frees=" << stats.frees
               << " device_operations=" << stats.deviceOperations << " synchronizations=" << stats.synchronizationEvents
               << " errors=" << stats.errors << " warnings=" << stats.warnings
               << " pending_device_operations=" << stats.pendingDeviceOperations
               << " dropped_device_operations=" << stats.droppedDeviceOperations << '\n';
    }
    if (synccheck_ != nullptr) {
        const SynccheckStats stats = synccheck_->Stats();
        const uint64_t errors = stats.duplicateOpens + stats.unmatchedCloses + stats.unconsumedOpens;
        output << "tool=synccheck sync_events=" << stats.syncEvents
               << " synchronizations=" << stats.synchronizationEvents << " matched_pairs=" << stats.matchedPairs
               << " duplicate_opens=" << stats.duplicateOpens << " unmatched_closes=" << stats.unmatchedCloses
               << " unconsumed_opens=" << stats.unconsumedOpens << " invalid_events=" << stats.invalidEvents
               << " pending_opens=" << stats.pendingOpens << " errors=" << errors << " warnings=0" << '\n';
    }
    output << "callbacks=" << callbackCount_.load() << " malformed_callbacks=" << malformedCallbacks_
           << " framework_errors=" << frameworkErrors_ << " dropped_messages=" << server_.DroppedMessages();
    return output.str();
}

} // namespace npu::sanitizer
