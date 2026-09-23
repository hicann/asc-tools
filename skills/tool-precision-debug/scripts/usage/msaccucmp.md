# msaccucmp：输入输出 Dump 转换与比较

使用 CANN 配套的 `msaccucmp.py` 将模型或单算子的输入输出 Dump 转为可读取的 Tensor 文件，并进行官方指标比较。它也支持直接比较两份独立 `.npy`。

## 选择操作

| 当前输入与目的 | 子命令 | 结果 |
| --- | --- | --- |
| 已采集的输入输出 Dump，要读取 Tensor 或转换布局 | `convert` | 指定格式的 Tensor 文件 |
| 网络或单算子 Dump，要开展对应场景的比较 | `compare` | 官方比较结果 |
| 两份独立 `.npy`，要比较 actual 与 golden | `file_compare` | 两份 Tensor 的比较指标 |
| 已按工具要求完成溢出采集，要分析该数据 | `overflow` | 对应的溢出分析结果 |

将工程实际使用的 CANN 安装根记为 `<cann-root>`，使用该安装中的工具。下面示例依据 CANN 9.0.0；执行时先查看对应子命令的 `--help`，确认当前版本接受的输入与选项。

## 转换 Dump

```text
python "<cann-root>/tools/operator_cmp/compare/msaccucmp.py" convert \
  -d "<输入输出dump>" -out "<artifact-dir>/converted" -t npy
```

| 参数 | 含义 |
| --- | --- |
| `-d` | 输入 Dump 文件或工具接受的文件目录 |
| `-out` | 输出目录 |
| `-t` | 输出文件类型，例如 `npy` |
| `-f` | 目标 Tensor 布局；需要 Format 转换时，按当前工具支持的布局填写 |

`-t` 选择文件表示，`-f` 选择 Tensor 布局，两者用途不同。文件转换见[查看 Dump 数据](https://www.hiascend.com/document/detail/zh/canncommercial/900/devaids/ModelAccuracyAnalyzer/atlasaccuracy_16_0055.html)，布局选项见[Dump Format 转换](https://www.hiascend.com/document/detail/zh/canncommercial/900/devaids/ModelAccuracyAnalyzer/atlasaccuracy_16_0054.html)。

按该版文档，`-d` 的目录形式读取所给父目录，不递归嵌套目录。转换后保留原始 dtype 和布局信息：例如 BF16 Dump 输出为 `.npy` 时可能转成 FP32，后续选择比较标准时仍需明确原输出 dtype。查看数值前核对 shape、布局与有效区。

## 比较两份 Tensor

```text
python "<cann-root>/tools/operator_cmp/compare/msaccucmp.py" file_compare \
  -m "<actual.npy>" -g "<golden.npy>" -out "<artifact-dir>/official-compare"
```

将实际输出与相同语义的参考文件配对，读取比较结果及日志。`compare` 和 `overflow` 的输入组织与该命令不同，按所用场景的配套帮助执行。

官方比较指标用于观察差异；正式验收仍按项目确定的完整判据解释。需要错值坐标、有效区或原始位模式时，将可比 Tensor 交给 [precision_compare.py](precision-compare.md)；需要确定标准或构造 golden 时使用 `/ops-precision-standard`，衔接[比较与验收](../../references/diagnosis/baseline.md)。

## 采集入口与格式

输入输出 Dump 通过工程实际使用的 Runtime 或框架入口采集。`dump_level=kernel` 表示输入输出 Dump 的采集层级；Kernel 内 `DumpTensor`、printf 等调测信息则由 `dump_kernel_data` 配置，并使用 [show_kernel_debug_data](kernel-debug-data-parse.md#采集与直接解析) 解析。两者即使都生成 `.bin`，解析路径也不同。[CANN 9.0.0 配置说明](https://www.hiascend.com/document/detail/zh/canncommercial/900/API/runtimeapi/aclcppdevg_03_0022.html)

自动输入输出 Dump 的支持范围按实际执行入口确认；裸 Kernel 直调需要核对其采集方式。工程自行保存的裸 Tensor `.bin` 按已知 dtype/shape 读取，使用[Tensor 比较工具](precision-compare.md)，而不是当作 Runtime Dump 转换。

采集文档若指定 `ascendc_parse_dumpinfo.py`，沿用其对应格式和用法。新增配置、转换结果和比较报告统一放在[任务产物目录](../../references/diagnosis/reporting.md#任务产物目录)下；原始 Dump 保留，工具结果与本轮输入及运行对应。
