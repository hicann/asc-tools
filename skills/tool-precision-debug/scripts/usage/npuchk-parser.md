# npu check 日志整理：本 Skill 辅助脚本

使用 CANN 配套的 `ascendc_tools/ascendc_npuchk_report.py` 分析 `*_npuchk.log`；用本脚本将一个或多个日志汇总为结构化摘要，保留错误标记、原始行、intrinsic、backtrace 和 core 身份。本脚本可离线读取已采集的日志，生成日志的运行方式见下方 [npu check](#npu-check)。

## 使用与输入

下文 `<artifact-dir>` 使用[任务产物目录](../../references/diagnosis/reporting.md#任务产物目录)的绝对路径，各输出均放在其内部。

```text
python "<skill-root>/scripts/debug/npuchk_parser.py" \
  --input <log-or-directory> --output "<artifact-dir>/npuchk.json" --cann <version>
```

目录默认扫描顶层，需要子目录时增加 `--recursive`。报告保留每个输入的相对路径、哈希、字节/行数、解码状态和观察数。规模限制由 `--max-*` 控制；具体值与输入校验见[当前实现](../debug/npuchk_parser.py)。生成新日志时按下方[npu check 操作](#npu-check)运行用户工程；安装不明或调用失败时再用[能力探测](asc-tools-probe.md)。

## 输出怎样解释

| 输出 | 含义与限制 |
| --- | --- |
| `OBSERVATIONS` | 有受支持的错误标记，仍需结合源码与数值后果解释 |
| `NO_OBSERVATIONS` | 已采集文本里没有解析器识别的标记；不能证明算子无错误 |
| `documented / unknown` | 标记名属于固定来源名单 / 扩展或未知标记；不代表严重度或根因 |
| `completeness.status=unknown` | 当前格式没有已验证的完整结束标记；不是等待自动改为完整的状态 |
| 返回 `0 / 2` | 报告有效并写出 / 输入、资源限制或调用无效 |

官方说明指出异常退出或 ASSERT 可能导致日志不完整。核对已采集 core/文件集合与本次预期执行集合；解析器不会猜测缺失的核数，也不会因文件数看似合理就证明完整。

原始告警、疑似工具限制和确认缺陷在调查记录中分别解释。数值通过不能单独证明误报，有告警也不能单独证明它造成当前精度失败。

## 依据与继续定位

参数、识别标记和报告字段以[当前实现](../debug/npuchk_parser.py)为准。日志与完整性限制见下方说明，当前工具文档使用 `/ascendc-docs-search` skill 查询。有告警时按[假设与实验](../../references/diagnosis/hypothesis-experiment.md)区分解释，CPU/NPU 的可比性见[差分方法](../../references/diagnosis/cpu-npu-differential.md)。

## npu check

npu check 随 CPU Debug 执行检查内存访问、Tensor 生命周期和同步关系。官方说明中的日志位于程序执行目录的 `npuchk/` 下，文件名以 `_npuchk.log` 结尾。工程允许独立工作目录时，可在 `<artifact-dir>/npuchk-run/` 运行；必须从工程目录执行时，将本轮日志收集到 `<artifact-dir>/npuchk/` 并注明原始位置。保留原始日志后，再用官方报告脚本检查：[9.0.0][npuchk-source-3]、[9.1.0][npuchk-source-4]

```text
python <ascendc_npuchk_report.py的实际路径> <日志路径>
```

常见入口是安装根下的 `tools/ascendc_tools/ascendc_npuchk_report.py`，或 asc-tools 源码中的 `npuchk/ascendc_npuchk_report.py`。不传日志参数时，脚本会在当前目录查找相应日志；为对应本次运行，通常明确指定文件。[报告脚本 9.0.0][npuchk-source-14]、[9.1.0][npuchk-source-15]

| 常见标记 | 官方说明中的含义 | 阅读时关注 |
| --- | --- | --- |
| `ErrorRead1/3/4`、`ErrorWrite1/2/4` | 非法访问、越界或地址对齐问题 | 对应地址、有效长度、分配与释放位置 |
| `ErrorRead2`、`ErrorWrite3` | 疑似读取未写数据、重复写入未被取走的数据 | 日志将这些标记为可疑问题，结合真实数据流判断 |
| `ErrorSync1/2` | 读写存在流水同步问题 | 生产者、消费者及相应 barrier 或 set/wait |
| `ErrorSync3/4` | set/wait 不配对或 eventID 使用冲突 | 同步方向、循环次数和尾轮路径 |
| `ErrorLeak`、`ErrorFree`、`ErrorBuffer0–4` | 泄漏、重复释放、Tensor/队列或缓冲区初始化问题 | 对应对象的生命周期与状态 |

这些含义来自两个版本相同的说明。新标记保留原文，再查当前版本，不套成已有错误。只有 CPU 调试正常结束、未被 ASSERT 等中断时，官方流程才会输出完整检查日志；没有错误打印不等于已经证明本次运行完整、数值正确。

已有日志需要结构化汇总时，可用本 Skill 的[日志解析器](npuchk-parser.md)，不要求本机安装 npu check。告警与精度异常的因果关系继续按[假设与实验](../../references/diagnosis/hypothesis-experiment.md)验证。

[npuchk-source-3]: https://gitcode.com/cann/asc-tools/blob/46a5b4d9ad924e5432e155cb62eea7585b448e62/docs/02_npu_check.md
[npuchk-source-4]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/docs/02_npu_check.md
[npuchk-source-14]: https://gitcode.com/cann/asc-tools/blob/46a5b4d9ad924e5432e155cb62eea7585b448e62/npuchk/ascendc_npuchk_report.py
[npuchk-source-15]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/npuchk/ascendc_npuchk_report.py
