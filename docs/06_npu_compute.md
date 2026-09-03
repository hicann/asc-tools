# npu-compute

## 概述

`npu-compute` 用于运行已编译的 NPU 应用并采集性能数据。采集成功后，工具将硬件信息和所选 Section 的 CSV 数据打包为 `.npu-rep` 报告；也可以将已有报告解包为可查看的文件。

当前支持以下 Section：

| Section | 说明 |
| :--- | :--- |
| `PipeUtilization` | 计算与流水线利用率数据 |
| `Memory` | 主存和片上存储访问数据 |
| `MemoryL0` | L0 存储访问数据 |
| `MemoryUB` | Unified Buffer 访问数据 |
| `L2Cache` | L2 Cache 访问数据 |

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
| `-h`、`--help` | 输出帮助信息并退出。与其他合法选项同时使用时，只输出帮助信息；存在错误选项时，先输出全部参数错误，再输出帮助信息。|
| `--list-sections` | 输出所有支持的 Section 并退出。只能单独使用；与 `-h` 或 `--help` 同时使用时，只输出帮助信息。|
| `--section <id>` | 指定一个采集 Section，可重复指定。采集命令至少需要一个 Section。|
| `--replay-mode <mode>` | 预留的采集模式选项，当前仅支持 `kernel`；不指定该选项时使用 `kernel` 模式。|
| `-o <path>`、`--export <path>` | 采集模式下指定报告输出位置；导入模式下指定解包结果的父目录。|
| `-i <report>`、`--import <report>` | 导入 `npu-compute` 生成的 `.npu-rep` 报告。|

同一个 `--section` 重复指定时只采集一次。`--replay-mode`、`--import` 和 `--export` 每条命令只能指定一次。

## 采集性能数据

采集命令必须指定至少一个 `--section` 和一个目标程序。

```bash
npu-compute \
  --section PipeUtilization \
  --section Memory \
  ./application
```

采集完成后，工具会在终端输出本次数据目录和报告路径：

```text
npu-compute: data-directory=<数据目录>
npu-compute: report=<报告路径>
```

数据目录位于执行命令时的当前目录，名称格式如下：

```text
npu-compute-<毫秒时间戳>-<进程ID>-<随机后缀>
```

每次采集使用独立的数据目录，因此多次调用不会混合采集文件。数据目录中包含 `HardwareInfo.jsonl` 和本次实际生成的 Section CSV 文件。

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
npu-compute: unpacked=<解包结果目录>
```

使用 `--export` 指定已有目录时，工具会在该目录下创建唯一的结果子目录：

```bash
mkdir -p ./restored
npu-compute --import ./reports/profile.npu-rep --export ./restored
```

解包结果目录包含打包前的采集文件，例如 `HardwareInfo.jsonl`、`PipeUtilization.csv` 和 `Memory.csv`。导入不会运行目标程序。

`--import <report>` 可以单独使用；需要指定解包结果的保存目录时，可以同时使用 `--export <已存在目录>`。

## 报告内容

报告可包含以下文件：

| 文件 | 内容 |
| :--- | :--- |
| `HardwareInfo.jsonl` | 默认采集的主机和 NPU 硬件信息。|
| `<Section名称>.csv` | 已选择 Section 的性能数据。对应 Section 获得有效 PMU 数据行时生成。|

不同 Section 的 CSV 字段不同。分析数据时，应以 CSV 首行的字段名为准。如果某个 Section 未获得有效 PMU 数据行，则对应 CSV 可能不会生成；其他已经生成的有效文件仍可写入报告。

## 约束说明

- 目标程序必须能在未使用 `npu-compute` 时独立运行，并在运行过程中至少成功执行一次 NPU 核函数。
- 目标程序必须是可执行文件；脚本也可以通过 `bash` 等解释器作为目标程序运行。
- 目标程序或其启动的脚本不能再次运行 `npu-compute` 进行采集。检测到嵌套采集时，本次采集失败且不会生成报告。
- 一条采集命令只运行一个目标程序。需要采集多个程序时，分别执行多条命令。
- `--list-sections` 不能与采集、导入或其他配置选项组合。
- `--` 不是受支持的选项分隔符；请将目标程序直接放在工具选项之后。
- 采集和导入均不会覆盖已有报告文件或结果目录。
