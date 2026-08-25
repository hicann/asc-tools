# Synccheck Samples

该目录中的每个同名子目录都是一个可独立构建和验证的 synccheck 样例包，包含 `.asc`、
`CMakeLists.txt`、`run.sh` 和 `verify.py`。`.asc` 直接包含同步指令序列、数据搬运上下文，
以及 ACL 初始化、设备内存、kernel launch、stream 和清理逻辑。

## 运行

从仓库任意目录调用顶层 demo runner，参数格式为 `synccheck/<文件名>`，不包含 `.asc` 后缀：

```bash
bash npu_sanitizer/demo/run.sh synccheck/single_pair
bash npu_sanitizer/demo/run.sh synccheck/single_unconsumed
```

runner 先调用对应子目录的 `run.sh` 独立构建 `aclsan_demo_synccheck_<文件名>`，使用
`examples/synccheck/symbol_ordering.txt` 插桩，再通过 `npu_check --tool synccheck` 启动样例。
原始 `npu_check` 输出保持在终端显示，同时写入用例构建目录，最后由该目录的 `verify.py`
核对退出状态、summary、诊断类型和会话生命周期。

正常样例返回 0。能够被 synccheck 检出的异常样例会输出诊断，并由 `npu_check` 返回非零状态。
`wait_without_set` 和 `mutex_duplicate_lock` 会使设备指令保持阻塞，因此它们使用有界 stream
同步并强制销毁 stream，避免异常演示进程无法退出。

## 样例

| 目录 | 预期 | 场景 |
| --- | --- | --- |
| `no_sync/` | 正常 | 仅执行非同步内存指令。 |
| `single_pair/` | 正常 | 一组匹配的 `SET_FLAG -> WAIT_FLAG`。 |
| `single_unconsumed/` | 异常 | 一个未消费的 `SET_FLAG`。 |
| `duplicate_set/` | 异常 | 相同 key 连续执行两次 `SET_FLAG`。 |
| `multi_block_isolation/` | 异常 | 两个 block 使用相同 key，仅 block 0 等待。 |
| `wait_without_set/` | 异常 | 没有对应 `SET_FLAG` 的 `WAIT_FLAG`；需超时同步并强制销毁阻塞的 stream。 |
| `mutex_pair/` | 正常 | MTE2 和 V 流水分别执行匹配的 `GET_BUF -> RLS_BUF`。 |
| `mutex_unreleased/` | 异常 | `GET_BUF` 后未执行 `RLS_BUF`。 |
| `mutex_unlock_without_lock/` | 异常 | 没有对应 `GET_BUF` 的 `RLS_BUF`。 |
| `mutex_duplicate_lock/` | 异常 | 相同 mutex ID 连续执行两次阻塞式 `GET_BUF`。 |
| `mutex_multi_block_isolation/` | 异常 | 两个 block 使用相同 mutex ID，仅 block 0 释放。 |
