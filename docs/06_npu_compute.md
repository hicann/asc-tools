# npu-compute

## 概述

`npu-compute` 用于运行已编译的 NPU 应用并采集性能数据。采集成功后，工具将硬件信息、结果摘要和所选 Section 的采集数据打包为 `.npu-rep` 报告；也可以将已有报告解包为可查看的文件。

当前支持以下 Section：

| Section | 说明 |
| :--- | :--- |
| `PipeUtilization` | 计算与流水线利用率数据 |
| `Memory` | 主存和片上存储访问数据 |
| `MemoryL0` | L0 存储访问数据 |
| `MemoryUB` | Unified Buffer 访问数据 |
| `L2Cache` | L2 Cache 访问数据 |
| `ArithmeticUtilization` | Cube FP/INT 指令数与 Cube、Vector 活跃周期占比 |
| `ResourceConflictRatio` | 流水等待率与 Vector 资源冲突率 |
| `Pipeline` | 流水 Timeline 数据 |

Section 名称区分大小写。

## 环境准备

请参考[快速入门](00_quick_start.md)完成环境准备。使用前，请安装与目标 NPU 和驱动匹配的 CANN 软件包，并加载 CANN 环境变量。以下命令中的 `<CANN安装目录>` 替换为实际安装目录：

```bash
source <CANN安装目录>/cann/set_env.sh
```

运行以下命令，检查 `npu-compute` 是否可以正常调用：

```bash
npu-compute --help
```

目标程序应能够在当前环境中独立运行，并在运行过程中至少成功执行一次 NPU 核函数。

## 命令格式

```text
npu-compute [options] [program] [program-arguments]
```

`program` 是待采集的目标程序，可以通过绝对路径、相对路径或命令名指定。使用命令名时，该程序必须能够通过 `PATH` 环境变量找到。目标程序可以是编译生成的可执行文件，也可以是具有执行权限的脚本。目标程序后的所有内容均作为目标程序参数传递。

目标程序中待采集的算子当前仅支持使用 Ascend C 编写，并通过 `<<<>>>` 方式调用。芯片支持范围及其他限制请参见[约束说明](#约束说明)。

例如：

```bash
npu-compute --section PipeUtilization ./application --input input.bin
```

工具选项必须位于目标程序之前。

目标程序必须具有执行权限。脚本没有执行权限时，应显式调用对应解释器，例如：

```bash
npu-compute --section PipeUtilization bash ./run.sh
```

### 选项

| 选项 | 说明 |
| :--- | :--- |
| `-h`、`--help` | 输出帮助信息。 |
| `--list-sections` | 列出支持的指标组名称，单独使用。 |
| `--list-sets` | 列出 basic、full 及其包含的 Section，单独使用，不启动采集。 |
| `--set arg` | 选择 basic 或 full，区分大小写。未指定 `--set` 和 `--section` 时默认使用 basic。支持重复指定，可与 `--section` 组合。 |
| `--section arg` | 指定要采集的指标组（Section）名称，区分大小写。可多次使用该选项指定不同指标组，采集命令至少需要指定一个指标组。 |
| `--replay-mode arg` | Kernel 重放模式，当前仅支持 `kernel`，默认值为 `kernel`。 |
| `-o arg`、`--export arg` | 采集时指定报告文件路径或已有目录；导入时指定保存解包结果的已有目录，工具会在其中创建新的结果子目录。未指定时，报告或解包结果保存在当前目录。 |
| `-i arg`、`--import arg` | 导入并解包 npu-compute 生成的 `.npu-rep` 报告，可配合 `--export` 指定保存位置。 |

同一个 `--section` 重复指定时只采集一次。`--replay-mode`、`--import` 和 `--export` 每条命令只能指定一次。

## 采集性能数据

采集命令指定目标程序即可；未指定 `--section` 和 `--set` 时默认采集 basic。

`basic` 包含 Pipeline、PipeUtilization、Memory、MemoryL0、MemoryUB、L2Cache 和 ArithmeticUtilization；`full` 在此基础上增加 ResourceConflictRatio。

`--set` 可以与 `--section` 组合。工具按照参数出现顺序展开两者，并按首次出现顺序去重。例如 `--section Memory --set basic` 会优先采集 Memory，再按 basic 顺序补充其余 Section。`--set` 不能与 `--import` 组合；`--list-sets` 必须单独使用，且不能重复指定。

```bash
npu-compute --list-sets
npu-compute --set basic --section ResourceConflictRatio ./application
```

```bash
npu-compute \
  --section PipeUtilization \
  --section Memory \
  ./application
```

采集完成后，工具会在终端输出本次数据目录和报告路径：

```text
npu-compute: data-directory= <数据目录>
npu-compute: report= <报告路径>
```

数据目录位于执行命令时的当前目录，名称格式如下：

```text
npu-compute-<毫秒时间戳>-<进程ID>-<随机后缀>
```

每次采集使用独立的数据目录，因此多次调用不会混合采集文件。数据目录中包含 `HardwareInfo.jsonl`、`summary.jsonl` 和本次实际生成的 Section 数据文件。

### 指定报告输出位置

未指定 `--export` 时，报告保存在当前目录，名称格式如下：

```text
report_<毫秒时间戳>_<随机标识>.npu-rep
```

`--export` 可以指定以 `.npu-rep` 结尾的报告文件路径：

```bash
npu-compute --section PipeUtilization \
  --export ./reports/profile.npu-rep \
  ./application
```

报告文件的父目录必须已存在，且不会覆盖已有文件。

`--export` 也可以指定一个已存在的目录，工具会在其中生成自动命名的报告：

```bash
mkdir -p ./reports
npu-compute --section Memory --export ./reports ./application
```

### 采集多个 Section

可在同一条命令中指定多个 Section：

```bash
npu-compute \
  --section PipeUtilization \
  --section Memory \
  --section MemoryL0 \
  --section MemoryUB \
  --section L2Cache \
  --section ArithmeticUtilization \
  --section ResourceConflictRatio \
  --section Pipeline \
  ./application
```

CSV 中的 `NA` 表示该项指标未采集到有效值。其他已采集到的指标仍可正常查看。

## 导入报告

使用 `--import` 解包报告：

```bash
npu-compute --import ./reports/profile.npu-rep
```

未指定 `--export` 时，工具在当前目录创建唯一的解包结果目录，并输出路径：

```text
npu-compute: unpacked= <解包结果目录>
```

使用 `--export` 指定已有目录时，工具会在该目录下创建唯一的结果子目录：

```bash
mkdir -p ./restored
npu-compute --import ./reports/profile.npu-rep --export ./restored
```

解包结果目录包含打包前的采集文件，例如 `HardwareInfo.jsonl`、`summary.jsonl`、`PipeUtilization.csv` 和 `PipeTrace.json`。导入不会运行目标程序。

`--import arg` 可以单独使用；需要指定解包结果的保存目录时，可以同时使用 `--export <已存在目录>`。

## 报告内容

报告可包含以下文件：

| 文件 | 内容 |
| :--- | :--- |
| `HardwareInfo.jsonl` | 默认采集的主机和 NPU 硬件信息。|
| `summary.jsonl` | 默认采集的结果摘要。 |
| `PipeTrace.json` | 指定 `--section Pipeline` 时采集的流水 Timeline 数据，Chrome Trace 格式。 |
| `<Section名称>.csv` | 通过 `--section` 指定的 Section 的性能数据。对应 Section 获得有效 PMU 数据行时生成。|

不同 Section 的 CSV 字段不同。分析数据时，应以 CSV 首行的字段名为准。如果某个 Section 未获得有效 PMU 数据行，则对应 CSV 可能不会生成；其他已经生成的有效文件仍可写入报告。

`PipeTrace.json` 支持使用 Chrome Trace 兼容工具进行可视化。

## 约束说明

- 芯片类型：当前仅支持 Ascend 950（dav-3510）。
- 编程语言：待采集的算子当前仅支持 Ascend C。
- Kernel 函数类型：目前需使用 `__cube__`、`__vector__` 或 `__mix__(cube,vec)` 限定符指定 Kernel 类型。
- 算子调用方式：当前仅支持通过 `<<<>>>` 方式进行单算子单次调用。
- `Pipeline` Section 基于采样机制生成流水图，与用户启用的核数无直接关系；即使启用全部核，流水图最多展示 6 个核的数据。
- 目标程序必须能在未使用 `npu-compute` 时独立运行，并在运行过程中至少成功执行一次 NPU 核函数。
- 目标程序必须是可执行文件；脚本也可以通过 `bash` 等解释器作为目标程序运行。
- 目标程序或其启动的脚本不能再次运行 `npu-compute` 进行采集。检测到嵌套采集时，本次采集失败且不会生成报告。
- 一条采集命令只运行一个目标程序。需要采集多个程序时，分别执行多条命令。
- `--list-sections` 不能与采集、导入或其他配置选项组合。
- 采集和导入均不会覆盖已有报告文件或结果目录。
