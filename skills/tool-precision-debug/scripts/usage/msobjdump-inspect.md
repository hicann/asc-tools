# ELF 结构检查：本 Skill 的 msobjdump 适配器

调用官方 msobjdump 检查 ELF，将结构字段、原始输出与执行信息保存为 JSON。适配器提供 dump/list 操作，适合核对 Kernel 和 metadata；直接查看或提取设备文件时，使用下方[官方命令](#直接使用-msobjdump)。

## 使用与输入

下文 `<artifact-dir>` 使用[任务产物目录](../../references/diagnosis/reporting.md#任务产物目录)的绝对路径，各输出均放在其内部。

已知可用的 msobjdump 路径时直接调用：

```text
python "<skill-root>/scripts/debug/msobjdump_inspect.py" \
  --tool <msobjdump可执行文件路径> \
  --input <kernel-elf> --operation dump --cann <version> \
  --output "<artifact-dir>/elf-observations.json"
```

需要列出容器内文件时把 operation 改为 `list`。报告自动记录输入与工具入口文件的路径、哈希、运行命令、日志和退出状态。`--cann` 是调用者声明的版本；没有 probe 时，`probe_report` 为 `null`，不会声称安装版本已确认。可用参数和资源上限见[当前实现](../debug/msobjdump_inspect.py)。

安装不明时再用[探测器](asc-tools-probe.md)指定 `--expected-cann <version> --require msobjdump`。需要绑定已有报告时，在上述命令增加 `--probe-report "<artifact-dir>/probe.json"`；适配器会核对声明版本、能力和工具入口，报告无效或不匹配会报错，不自动退回无绑定调用。

## 输出怎样解释

- `OBSERVATIONS` 表示提取到支持的结构行；字段存在不证明该分支在失败输入上执行过。
- `NO_OBSERVATIONS` 表示未识别到支持字段；例如 list 输出 `nothing to list` 不能证明不存在 metadata。
- `TOOL_ERROR` 连同原始 diagnostics 保留；不要通过重命名输入等方式把异常结果包装为等价证据。
- 字段的版本语义与保留值见下方[msobjdump 操作说明](#直接使用-msobjdump)，结构提取与运行分支是否采用分别判断。

## 官方语义与产物采用

命令与字段解释见下方说明，当前工具文档和示例使用 `/ascendc-docs-search` skill 查询；适配器参数和报告以[当前实现](../debug/msobjdump_inspect.py)为准。将结构报告与构建、加载记录关联，按[复现性方法](../../references/diagnosis/reproducibility.md)确认产物；数值定位继续使用[Checkpoint 方法](../../references/diagnosis/checkpoint-design.md)。

## 直接使用 msobjdump

msobjdump 用于读取或提取算子 ELF 中的设备信息。下面的常用命令在两个版本的文档中一致，按当前需要选择一条即可。[9.0.0][msobjdump-source-5]、[9.1.0][msobjdump-source-6]

```text
msobjdump --dump-elf <ELF>
msobjdump --dump-elf <ELF> --verbose
msobjdump --list-elf <ELF>
msobjdump --extract-elf <ELF> --out-dir "<artifact-dir>/elf-extract"
```

| 操作 | 作用 |
| --- | --- |
| `--dump-elf` | 打印工具能识别的 Kernel 和 metadata 字段 |
| `--verbose` | 补充 ELF header、section、symbol 等详细信息 |
| `--list-elf` | 列出容器内可列举的 ELF 文件 |
| `--extract-elf` | 将支持提取的设备文件写到输出目录 |

例如，`.ascend.meta`、`KERNEL_TYPE`、`FUNCTION_ENTRY` 可帮助检查 Kernel、核类型和 TilingKey 的结构信息；但字段存在不证明失败输入实际执行过这个分支。`BLOCK_NUM` 在这两个固定实现中打印保留值 `0xFFFFFFFF`，不能作为实际执行核数。[实现 9.0.0][msobjdump-source-16]、[9.1.0][msobjdump-source-17]

输出受工具版本和 ELF 格式影响。9.1.0 新增 `.aicore_binary` 处理路径，该路径还使用 `llvm-objcopy`；旧工具未识别某种格式，不能据此断言 ELF 中没有设备信息。需要保存结构化字段和原始输出时，使用本 Skill 的[ELF 检查适配器](msobjdump-inspect.md)，它只开放 dump/list 操作。产物是否被本次运行采用，见[复现性方法](../../references/diagnosis/reproducibility.md)。

[msobjdump-source-5]: https://gitcode.com/cann/asc-tools/blob/46a5b4d9ad924e5432e155cb62eea7585b448e62/docs/03_msobjdump.md
[msobjdump-source-6]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/docs/03_msobjdump.md
[msobjdump-source-16]: https://gitcode.com/cann/asc-tools/blob/46a5b4d9ad924e5432e155cb62eea7585b448e62/utils/msobjdump/msobjdump/msobjdump_main.py
[msobjdump-source-17]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/utils/msobjdump/msobjdump/msobjdump_main.py
