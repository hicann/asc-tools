# 调试工具选用

主动使用工具获取观察、验证假设和检查修复。根据当前问题选择下面的入口，阅读用法后执行；已有日志、Tensor 和比较报告可以直接作为输入。同一能力优先使用工程现有命令和当前 CANN 配套工具，分析脚本可以与它们组合使用。

本页说明**什么时候用、能得到什么、去哪里看用法**。命令参数、格式要求和结果解释放在各工具的使用说明中；定位思路与故障经验见[知识目录](../references/index.md)。

## 按当前问题选择

| 当前问题 | 使用工具与用法 | 得到什么 |
| --- | --- | --- |
| 已有输出和 golden，要知道具体错在哪里 | [precision_compare.py](usage/precision-compare.md) | 错值坐标、误差分布、有效区与位模式差异 |
| 同一输入时好时坏，或错误位置、幅度在变化 | [repeatability_analyzer.py](usage/repeatability-analyzer.md) | 多轮结果、失败率及稳定或漂移的线索 |
| 需要观察 Kernel 内部 Tensor | [DumpTensor 插桩](../references/diagnosis/dumptensor.md) → [show_kernel_debug_data 解析](usage/kernel-debug-data-parse.md#采集与直接解析) | 指定观察点的中间值，供局部比较与根因验证 |
| 已有模型或单算子的输入输出 Dump，要转换或比较 | [msaccucmp.py](usage/msaccucmp.md) | 可读取的 Tensor 文件与官方比较指标 |
| 已有多个中间点的比较报告，要缩小调查范围 | [checkpoint_analyzer.py](usage/checkpoint-analyzer.md) | 各点状态、误差趋势与下一观察位置的建议 |
| 要跟踪控制流、索引或变量值 | [CPU Debug / gdb](usage/asc-tools-probe.md#cpu-debug) | CPU 执行路径、断点和变量观察 |
| 怀疑内存访问、Tensor 生命周期或同步有误 | CPU 域用 [npu check](usage/npuchk-parser.md#npu-check)；设备域用 [msSanitizer](../references/diagnosis/cpu-npu-differential.md#cpu-与设备检查) | 相应执行域中的访存、竞争或同步检查信息 |
| 要检查 ELF 中的 Kernel、架构或结构信息 | [msobjdump](usage/msobjdump-inspect.md#直接使用-msobjdump) | ELF、Kernel 和 metadata 信息 |
| 需要确定精度标准或构造 golden | 使用 `/ops-precision-standard`，衔接[比较与验收](../references/diagnosis/baseline.md#确认精度比对标准) | 与算子语义对应的比较规则、检查函数和参考构造方法 |

常用组合是：已有输出先分析错值分布；根据疑点采集中间值，再比较相关节点；结果不稳定时固定输入做重复运行；出现访存或同步线索时运行对应检查器。按新观察推进调查，选择能回答下一问题的工具。

## 运行、环境与结果整理

| 需要完成的工作 | 使用工具与用法 | 得到什么 |
| --- | --- | --- |
| 执行构建、复现或回归并保存过程 | [run_with_evidence.py](usage/run-with-evidence.md) | 完整日志、真实退出码、耗时与执行信息 |
| 查看实际运行端的环境身份 | [detect_profile.py](detect_profile.py)，运行前查看 `--help` | 可见的 CANN、SoC、架构和编程模型线索 |
| 确认 CANN 工具的安装位置和可用入口 | [asc_tools_probe.py](usage/asc-tools-probe.md) | 工具路径、组件和版本探测结果 |
| 汇总多个 npu check 日志 | [npuchk_parser.py](usage/npuchk-parser.md) | 错误标记、原始行、调用栈及核信息 |
| 留存结构化 ELF 检查结果 | [msobjdump_inspect.py](usage/msobjdump-inspect.md) | 官方输出、结构字段与执行记录 |
| 隔离解析路径并保留解析失败时的产物 | [kernel_debug_data_parse.py](usage/kernel-debug-data-parse.md) | 官方解析结果、原始输入对应关系与错误日志 |
| 关联插桩、采集、解析和源码恢复 | [dump_session.py](usage/dump-session.md) | 同一次采集过程各步骤的记录与恢复状态 |

已有可用命令或工具路径时，直接调用。安装不明或调用失败时，用探测器查明具体入口；某项工具不可用时，使用当前可行的采集或分析方式继续，并记录尚缺的观察。

## 使用约定

- **路径与产物**：将当前 `SKILL.md` 所在目录解析为 `<skill-root>`，将[任务产物目录](../references/diagnosis/reporting.md#任务产物目录)解析为 `<artifact-dir>`。日志、比较结果、dump 和临时脚本放在产物目录中，新一轮实验使用独立文件或子目录；输入保留原位。
- **执行环境**：使用工程已有的 Python 与实际采用的 CANN 安装。自有脚本支持 Python 3.11–3.13；数值比较使用 NumPy，仅整理外部 JSON 结果时按相应用法准备依赖。环境查询在实际运行端执行，长任务按[后台执行与停滞处理](../references/diagnosis/reproducibility.md#执行停滞与证据保存)保留日志和退出状态。
- **解释结果**：正式验收沿用项目确定的比较入口；诊断工具用于解释错值、分布、位模式和趋势，分析结果与原验收报告一并保留。比较方式与判据的区别见[比较与验收](../references/diagnosis/baseline.md)。

具体 API、编译方法、工具版本支持或样例查询使用 `/ascendc-docs-search`；产品映射与架构能力查询使用 `/npu-arch`。

## 脚本单元测试

单测位于 Skill 根目录的 `tests/`，依赖见 [tests/requirements.txt](../tests/requirements.txt)。使用已有 Python 环境执行 `python -B -m pytest <skill-root>/tests -q -p no:cacheprovider`；测试面向 Linux/macOS，使用临时数据、模拟外部工具和隔离后的环境变量，不需要 CANN 安装、CMake 或 NPU。
