# 场景与检查类型选择

先识别算子风险，再选择与风险对应的最小检查类型集合。检查类型选择依据是待检查的问题类型，不是算子使用的
某个 API 名称。

选择检查类型前先确认应用符合[适用范围与限制](applicability.md)。

## 场景判断

| 现象或代码变更 | 使用 `npu-check` 的目的 | 选择 |
| --- | --- | --- |
| 新增算子，准备检查正常、边界和尾块 shape | 发现精度结果可能掩盖的 Device 访问或同步问题 | 按风险选择，必要时同时启用 |
| 修改 Tiling、GM offset、搬运长度、stride、block 数或尾块处理 | 检查受支持数据搬运指令的 GM 读写是否越界 | `--tool memcheck` |
| AICore Error 或结果偶发异常，怀疑地址计算或数据搬运 | 获取访问方向、大小、源码位置和执行上下文 | `--tool memcheck` |
| 修改 `SetFlag/WaitFlag`、event ID、流水或 buffer 获取释放 | 检查 SET/WAIT 或 GET/RLS 的标识和执行次数是否匹配 | `--tool synccheck` |
| 部分 shape 卡住，怀疑条件分支或循环造成同步不对称 | 查找无匹配操作或未消费操作 | `--tool synccheck` |
| 同时修改数据搬运和同步逻辑 | 分开验证两类 Device 风险 | `--tool memcheck --tool synccheck` |

## Memcheck 能力与边界

`memcheck` 用于检查工具在本次运行中报告的 GM 访问。报告可提供：

- `read` 或 `write` 访问方向及字节数；
- 源码文件和行号，或 kernel 名称与 PC；
- AIC/AIV、core、block、pipe 和 launch 等执行上下文；
- 异常地址，以及特定错误中的分配基址、大小或生命周期信息；
- Device Frames，用于定位到用户算子中的调用点。

这些信息适合验证 Tiling 边界、GM offset、搬运长度、stride、block 划分和尾块处理。能力与结论边界见
[适用范围与限制](applicability.md)。

## Synccheck 能力与边界

当前配对检查包括：

```text
SET_FLAG -> WAIT_FLAG
GET_BUF  -> RLS_BUF
```

这两组关系是同步指令的硬件使用限制，不只是检查器的建议。每条实际执行路径上的配对双方必须使用相同 pair key、
执行相同次数并完成配对；分支、循环、尾块和提前返回都不能留下单独的 `SET_FLAG`、`WAIT_FLAG`、`GET_BUF` 或
`RLS_BUF`。

报告将违规归纳为：

- `duplicate_opens`：同一 key 的 `SET_FLAG` 或 `GET_BUF` 尚未执行对应的 `WAIT_FLAG` 或 `RLS_BUF`，就再次执行；
- `unmatched_closes`：`WAIT_FLAG` 或 `RLS_BUF` 没有相同 key 的 `SET_FLAG` 或 `GET_BUF`；
- `unconsumed_opens`：Runtime 同步结算时，`SET_FLAG` 或 `GET_BUF` 仍未执行对应的 `WAIT_FLAG` 或 `RLS_BUF`。

判断是否匹配时应读取报告中的 pair kind 和 pair key，而不是只比较操作名称。报告中的 launch 字段用于标识
当前诊断所属的 kernel 下发，不能据此扩大工具的适用范围。

## 运行选择规则

用户明确指定检查类型时遵循其选择；若任务证据表明该检查类型无法覆盖目标风险，应说明边界。未提供任何
`--tool` 时当前 CLI 默认启用 `memcheck`，但显式写出检查类型更便于审计本次检查范围。

疑似卡死且可以修改测试应用时，可将 `aclrtSynchronizeStream` 临时替换为
`aclrtSynchronizeStreamWithTimeout(stream, 10000)`，让 Host 最多等待 10 秒后返回以便保留定位信息；这不代表
Device 任务已经终止。超时后在失败清理路径调用 `aclrtDestroyStreamForce(stream)`，强制销毁阻塞的 stream，
使测试程序快速退出；外层仍应复用应用已有测试预算作为最终保护。具体用法和资源生命周期边界见
[运行 npu-check](execution.md)。
