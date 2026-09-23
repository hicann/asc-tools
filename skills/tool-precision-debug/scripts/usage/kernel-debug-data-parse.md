# Kernel dump 解析：本 Skill 的解析适配器

已有 Kernel 调测数据时，用官方 `show_kernel_debug_data` 解析；本适配器负责输入隔离、路径处理和失败产物留存，直接调用见[使用与输入](#使用与输入)。模型/单算子输入输出 Dump 使用 [msaccucmp.py](msaccucmp.md)，按采集来源选择解析器。更多工具见[工具入口](../README.md#按当前问题选择)。

## 使用与输入

下文 `<artifact-dir>` 使用[任务产物目录](../../references/diagnosis/reporting.md#任务产物目录)的绝对路径，各输出均放在其内部。

已有可用的官方解析器路径时直接调用：

```text
python "<skill-root>/scripts/debug/kernel_debug_data_parse.py" \
  --tool <show_kernel_debug_data可执行文件路径> \
  --input <bin-file-or-directory> --artifact-dir "<artifact-dir>/parsed" \
  --cann <version> --output "<artifact-dir>/parse-report.json"
```

目录按官方行为递归收集 `.bin`。适配器先记录输入路径、大小与哈希，再复制到私有 ASCII 临时目录解析，保护原始输入并隔离空格/非 ASCII 路径差异。输出目录需要是新的；超时和规模限制见[当前实现](../debug/kernel_debug_data_parse.py)及 `--help`。

报告自动记录工具入口文件的路径与哈希、执行命令、日志、退出状态和解析产物。`--cann` 是调用者声明的版本；没有 probe 时，`probe_report` 为 `null`，不会声称安装版本已确认。

安装不明时再用[探测器](asc-tools-probe.md)指定 `--expected-cann <version> --require show-kernel-debug-data`。需要绑定报告时，增加 `--probe-report "<artifact-dir>/probe.json"`；会核对声明版本、能力和解析器入口，报告无效或不匹配会报错，不自动退回无绑定调用。

## 输出怎样解释

| 状态或返回码 | 含义 |
| --- | --- |
| `OBSERVATIONS` | 有解析产物或支持的控制台边界，不证明 dump 完整或数值正确 |
| `NO_OBSERVATIONS` | 两者均未出现，保留原始输出进一步判断 |
| 返回 `0` | 报告有效，未检测到底层工具错误，仍可能没有观察 |
| 返回 `1` / `TOOL_ERROR` | 底层非零、超时、输出上限或显式异常标记；局部产物仍保留 |
| 返回 `2` | 调用、输入、探测绑定、资源或报告无效 |

适配器同时检查退出码与已知异常输出；底层返回 0 不是唯一成功依据。相关版本差异见下方[判断解析是否成功](#判断解析是否成功)。

Tensor 二进制/文本、`index_dtype.json`、时间戳和 parser log 分别保留。printf 文本留在原始输出，不由本工具自动转换成数值事实。

## 官方来源与下一步

生产端配置与解析用法见下方[采集与直接解析](#采集与直接解析)，当前适配器的绑定和失败处理见[实现](../debug/kernel_debug_data_parse.py)。需要当前接口或官方示例时，使用 `/ascendc-docs-search` skill。比较前核对 dtype、layout、有效区和 oracle，操作见[精度比较器](precision-compare.md)；观察解释与恢复见[DumpTensor 方法](../../references/diagnosis/dumptensor.md)。

## 采集与直接解析

采集与解析是两个步骤：先让实际运行的 Kernel 输出调试数据，再用 `show_kernel_debug_data` 离线解析。已有 dump 时可直接进入解析，无需再次运行算子。[9.0.0][parser-source-7]、[9.1.0][parser-source-8]

### 配置与采集

需要 Tensor 中间值时，先按上方 [DumpTensor 操作](../../references/diagnosis/dumptensor.md#最小插桩示例)补充调用和编译开关，再通过工程已有的 ACL 初始化或框架配置入口加载下面的配置。新建采集配置时保存到 `<artifact-dir>/acl-dump.json`，先将占位符替换为产物目录的绝对路径。仅创建 JSON 文件不会自动启用采集；已有框架管理 ACL 时使用其配置入口。

```json
{
  "dump": {
    "dump_kernel_data": "tensor",
    "dump_path": "<artifact-dir>/dump"
  }
}
```

`dump_kernel_data` 可选择 `tensor`、`printf`、`assert`、`timestamp` 或 `all`，多个类型用英文逗号分隔。目录优先级为 `ASCEND_DUMP_PATH > ASCEND_WORK_PATH > dump_path`。运行前确认实际采用的目录，运行后把本轮新产生的文件与输入、Kernel 产物对应起来。

### 解析与输出

```text
show_kernel_debug_data "<artifact-dir>/dump" "<artifact-dir>/parsed-direct"
```

输入目录会递归收集 `.bin`；输出目录可省略，默认使用当前工作目录。本 Skill 中显式指定产物目录内的新解析子目录，保留原始 dump。解析结果可能包含 Tensor 二进制/文本、`index_dtype.json`、时间戳、parser log 或控制台 printf 内容，具体取决于采集数据。

比较前核对 Tensor 的 dtype、layout、有效区和中间 oracle。printf 中有限位数的文本不能还原完整位模式；文件存在也不证明期望的每个核、每个观察点都已采到。

### 判断解析是否成功

同时查看退出码、异常输出和实际产物：9.0.0 的部分解析异常只打印 traceback，命令仍可能返回 0；9.1.0 对应异常返回 255，并由命令入口传递。9.1.0 的目录解析遇到首个失败会结束，所以失败时仍可能留下部分产物。[解析实现 9.0.0][parser-source-9]、[9.1.0][parser-source-10]；[命令入口 9.0.0][parser-source-11]、[9.1.0][parser-source-12]

9.1.0 对目录失败和含空格路径的处理有变化，并检查输出路径为 ASCII。任务目录不符合直接解析要求时，使用上方适配器在内部临时目录解析，再将结果写回指定产物目录；具体限制使用 `/ascendc-docs-search` skill 核对当前工具文档。

[parser-source-7]: https://gitcode.com/cann/asc-tools/blob/46a5b4d9ad924e5432e155cb62eea7585b448e62/docs/04_show_kernel_debug_data.md
[parser-source-8]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/docs/04_show_kernel_debug_data.md
[parser-source-9]: https://gitcode.com/cann/asc-tools/blob/46a5b4d9ad924e5432e155cb62eea7585b448e62/utils/show_kernel_debug_data/show_kernel_debug_data/dump_parser.py
[parser-source-10]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/utils/show_kernel_debug_data/show_kernel_debug_data/dump_parser.py
[parser-source-11]: https://gitcode.com/cann/asc-tools/blob/46a5b4d9ad924e5432e155cb62eea7585b448e62/utils/show_kernel_debug_data/show_kernel_debug_data/__main__.py
[parser-source-12]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/utils/show_kernel_debug_data/show_kernel_debug_data/__main__.py
