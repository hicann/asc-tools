# 运行 npu-check

选择检查类型后读取本参考。保留应用原有的工作目录、环境、Device 选择、参数和测试超时时间。不要编造应用
参数。如果项目已有 CANN 环境加载命令，运行前检查和 `npu-check` 必须复用该命令；否则使用
`source <CANN-install-dir>/cann/set_env.sh`。

## 运行前检查

运行前确认已安装工具和目标应用：

```bash
command -v npu-check
npu-check --help
test -x <application>
```

以当前工具的帮助输出作为命令行契约的权威依据。运行前还要确认：

- 应用满足[适用范围与限制](applicability.md)中的全部运行条件；
- 应用能用相同工作目录、参数和 Device 选择成功执行目标路径；
- 需要源码行号时，算子已按项目现有构建方式保留行号信息。

任一适用条件不满足时不运行。环境或命令异常时，保留原始报错并按[故障排查](troubleshooting.md)处理。

## 标准调用

使用已有应用命令替换 `<application>` 及其参数。参数包含空格时使用数组。模板统一添加 `--` 分隔符；当应用
路径或参数以 `-` 开头时，该分隔符不可省略。

```bash
capture_dir="$(mktemp -d)"
raw_log="${capture_dir}/npu-check.log"
application="<application>"
application_args=(<existing arguments>)

set +e
npu-check --tool memcheck \
  -- "${application}" "${application_args[@]}" >"${raw_log}" 2>&1
run_status=$?
set -e

printf 'npu-check exit status: %s\n' "${run_status}"
printf 'raw report: %s\n' "${raw_log}"
```

仅检查同步问题时，将检查类型参数替换为：

```bash
npu-check --tool synccheck
```

变更同时影响内存访问和同步时，检查类型参数应使用：

```bash
npu-check --tool memcheck --tool synccheck
```

所有变体都必须保留相同的应用命令、终端重定向和退出状态捕获。不要改写、摘录后覆盖或手工拼接原始日志；如果
需要摘录诊断，保留原始文件并在回答中注明摘录位置。

当前公开命令还支持 `--log-file <path>`。指定后，检查报告写入该文件，应用输出仍可能出现在终端。此时应分别
保留工具生成的原始报告、终端 stdout/stderr、执行命令和退出状态，并明确它们属于同一次执行。不要为了形成
单一文件而重新排序或拼接内容。

## 疑似卡死时的有界调用

如果测试应用卡在 `aclrtSynchronizeStream(stream)` 且允许修改应用，定位期间可临时改用 ACL Runtime 的
超时同步接口：

```cpp
aclError syncRet = aclrtSynchronizeStreamWithTimeout(stream, 10000);
if (syncRet == ACL_ERROR_RT_STREAM_SYNC_TIMEOUT) {
    aclError destroyRet = aclrtDestroyStreamForce(stream);
    if (destroyRet == ACL_SUCCESS) {
        stream = nullptr;
    }
    // 记录 syncRet 和 destroyRet，然后从失败路径退出。
}
```

`timeout` 参数单位为毫秒，`10000` 即建议的 10 秒。应用必须检查返回值；返回
`ACL_ERROR_RT_STREAM_SYNC_TIMEOUT` 时，记录超时并进入诊断错误路径，不要继续执行依赖同步完成的正常逻辑。
随后调用 `aclrtDestroyStreamForce(stream)`，可在不等待未完成任务的情况下强制销毁 stream，使测试程序快速进入
退出流程，避免普通销毁继续等待卡住的 Device stream。

`aclrtDestroyStreamForce` 仅用于失败清理：stream 必须由 `aclrtCreateStream` 或
`aclrtCreateStreamWithConfig` 创建，并且属于当前 Context。必须检查销毁结果；成功后句柄已经失效，不能再次使用
或调用 `aclrtDestroyStream`。若强制销毁失败，保留两个接口的返回值，并依赖外层测试预算终止进程。

同步接口超时本身不会终止 stream 上仍在执行或阻塞的 Device 任务。在强制销毁返回成功前，不得释放或复用该
任务仍可能访问的输入、输出和 workspace；强制销毁也不能解释为任务正常完成。终止前已经完整输出且上下文明确
的 synccheck 诊断仍可作为局部证据，但超时日志不能支撑完整通过结论。

外层仍应使用现有测试预算作为最终保护，且预算应覆盖应用内的 10 秒同步超时和日志收集时间。不要任意设置很长
的超时时间。保留其他参数和重定向，只将标准 `npu-check` 命令替换为以下形式：

```bash
set +e
timeout --signal=TERM --kill-after=5s <existing-test-budget> \
  npu-check --tool synccheck \
  -- "${application}" "${application_args[@]}" >"${raw_log}" 2>&1
run_status=$?
set -e
```

如果 `run_status` 是超时状态（通常为 `124`，强制终止后可能为 `137`），保留原始日志和退出状态，由 AI 按
[报告阅读](result-interpretation.md)判断其中哪些诊断可信、整体报告属于部分可靠还是不可靠。不得把超时后暂时
没有看到错误解释为通过。

## 结果判定规范

Shell 状态只能确认 CLI 进程如何退出，不能单独代表检查结论。应共同保留并提供给 AI：

- 未经改写的原始 stdout/stderr 或 `--log-file` 生成的原始报告；
- 完整执行命令、工作目录、检查类型、CANN/NPU 环境和工具版本；
- `run_status`，以及外层超时或信号终止信息；
- 与同一次执行对应的应用输出。

AI 根据这些原始证据判断报告可靠性，不使用固定文案、正则字段或字段存在性进行机械校验。报告来源不明、被手工
拼接、跨版本混用，或关键信息相互冲突时，应说明具体疑点并判断为不可靠；报告被截断但已有上下文充分的明确诊断
时，可判断为部分可靠并保留该诊断，但不能据此声明完整通过。
