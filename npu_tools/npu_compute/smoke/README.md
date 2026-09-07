# NPU Compute dav3510 冒烟基线

本目录是 `npu-compute` 在 dav3510 上的日常不可变冒烟基线，用于同时验证 Ascend C 样例、
工具安装、运行时采集、导出报告和结果校验。基线固定为下列六个 case，不从本地目录动态增删。

## 环境要求

- 已安装与 dav3510 匹配的 CANN 9.2（Bisheng 编译器）和 Driver，并且机器上有可用的
  dav3510 设备。
- 已执行目标 CANN 的 `set_env.sh`，环境变量 `ASCEND_HOME_PATH` 非空。
- 当前用户可构建 asc-tools 软件包，并有权限将生成的软件包安装到
  `${ASCEND_HOME_PATH}` 所在 CANN 目录。

全量入口会先构建并安装 asc-tools 软件包，再编译六个 case，因此无需预先单独构建工具或样例。
目标架构固定为 `dav-3510`。
每个样例固定启动 8 个 block；各 block 使用独立的输入和输出片段，避免以重复写入替代多 block 覆盖。

## 固定场景

| case | 场景 |
| --- | --- |
| `vector_add` | Vector Core 上的向量加法，校验基础 Vector 计算和搬运。 |
| `cube_mmad` | Cube Core 上的 16 x 16 矩阵乘，校验 `Mmad` 和 `Fixpipe`。 |
| `mix_1_1` | 一个 Cube Core 与一个 Vector Core 协同执行矩阵乘和 LeakyReLU。 |
| `mix_1_2` | 一个 Cube Core 与两个 Vector Core 协同执行矩阵乘和分片 LeakyReLU。 |
| `reg_add` | Vector Core 上的 `__simd_vf__` 寄存器编程加法。 |
| `simt_hello` | 启用 SIMT 编译选项的基础线程索引写回。 |

## 运行

从仓库根目录执行全量冒烟：

```bash
bash npu_tools/npu_compute/smoke/run_smoke.sh
```

默认入口会完整打包并安装当前工作区的 asc-tools。若要复用环境中已安装的 `npu-compute`、跳过
这一步，可执行：

```bash
bash npu_tools/npu_compute/smoke/run_smoke.sh --skip-asc-tools-build
```

跳过打包安装时，`NPU_COMPUTE_SMOKE_CLI` 可以指定绝对路径的可执行文件；未指定时使用 PATH 中的
`npu-compute`。两种方式都会重新编译并运行全部六个样例。

已安装可执行的 `npu-compute` 后，也可以分别运行单个 case：

```bash
bash npu_tools/npu_compute/smoke/examples/vector_add/run.sh
bash npu_tools/npu_compute/smoke/examples/cube_mmad/run.sh
bash npu_tools/npu_compute/smoke/examples/mix_1_1/run.sh
bash npu_tools/npu_compute/smoke/examples/mix_1_2/run.sh
bash npu_tools/npu_compute/smoke/examples/reg_add/run.sh
bash npu_tools/npu_compute/smoke/examples/simt_hello/run.sh
```

## Msprof 接口专项样例

`examples/mix_1_2_msprof` 基于 `mix_1_2`，用于单独验证 `MsprofStart`、`MsprofStop` 和原始
数据回调，不属于固定六 case，不会由 `run_smoke.sh` 自动执行。除加载目标 CANN 的 `set_env.sh`
外，运行前还需要将当前 asc-tools 安装到该 CANN，使
`${ASCEND_HOME_PATH}/<host>-linux/tools/npu_tools/lib64/libacl_tool_injection.so` 可用，其中 `<host>` 为
主机架构。

```bash
bash npu_tools/npu_compute/smoke/examples/mix_1_2_msprof/run.sh
```

该样例固定配置 `PROF_TASK_TIME_MASK | PROF_AICORE_METRICS_MASK`，不启用日志采集；将 10 个
PMU event 写入全部 PMU 槽位，并用 `PROF_COMPUTE_ALL_BLOCK` 请求 block 级数据。回调会分别打印
`msprof PMU task:` 和 `msprof PMU block:`，展示 task/stream、core、block/sub-block、cycle 与
PMU 计数值；结束时必须同时收到 task 和 block 两类 PMU record。

## npu-compute 与 msopprof 对比

以下入口会对六个固定 case（不含 `mix_1_2_msprof`）分别构建两次并比较共同的性能数据。通过
`--npu-compute-env` 和 `--msopprof-env` 分别传入两套 CANN 的 `set_env.sh`；两套 CANN 的构建目录、
可执行程序和运行时库不会复用。

```bash
bash npu_tools/npu_compute/smoke/compare_tools.sh \
  --npu-compute-env /path/to/npu-compute/cann/set_env.sh \
  --msopprof-env /path/to/msopprof/cann/set_env.sh \
  --output /tmp/npu-compute-msopprof-comparison
```

每个工具采集 `PipeUtilization`、`Memory`、`MemoryL0`、`MemoryUB` 与 `L2Cache`。每个 case 的
`comparison/<case>/comparison.csv` 按 `section`、`block_id`、`sub_block_id` 和共同字段列出原始值、
绝对差与相对差；`coverage.csv` 和 `summary.md` 记录缺失的 section、字段或行。此入口为仅报告模式，
性能数据差异不会导致失败；构建或采集命令失败仍会返回非零，并保留 `logs/` 中的工具日志。

每个 case 的 npu-compute 原始 CSV 先由工具写入日志中 `npu-compute: data-directory=` 指出的目录，
脚本随后复制到 `npu-compute/<case>/raw/`。采集成功后，脚本将同目录的 `.npu-rep` 用
`npu-compute --import --export` 解析到 `npu-compute/<case>/imported/`，并优先比较导入数据。若应用
结果校验失败、未生成 `.npu-rep`，仍使用 `raw/` 中已采集的 CSV 进行比较。

为使计数结果更明显，固定场景保持 8 block，并将 vector/reg 的每 block 元素数扩大到 4096，cube/mix
cube/mix 的每个 block 处理 32 个已验证的 `16 x 16 x 16` tile，SIMT 配置为每 block 256 线程、每线程写
64 个元素。

每个 case 都先调用 `npu-compute --list-sections`，并为返回的每一个 Section 传入一组
`--section <Section>`，不会只采集固定基线。当前必须可用的五个基线 Section 是：

- `PipeUtilization`
- `Memory`
- `MemoryL0`
- `MemoryUB`
- `L2Cache`

全量运行日志位于 `npu_tools/npu_compute/smoke/build/logs/<case>.log`。每个 case 的
`examples/<case>/build/` 下保存可执行程序、`npu_compute.log`、`result.npu-rep`，以及由
`npu-compute: data-directory=<绝对路径>` 输出指出的本次采集数据目录。

## 通过标准

单个 case 只有同时满足以下条件才打印 `[PASSED] <case>`：

- 编译和 `npu-compute` 采集命令均成功，应用输出精确的
  `result verification passed: <case>` 结果校验标记。每个 case 还必须为 8 个 block 分别输出
  `result verification passed: <case> block=<n>`；各 block 使用独立输入幅值并按独立期望值校验。
- `PipeUtilization.csv` 的核心行与场景一致：`vector_add`、`reg_add` 恰好出现
  `{vector0}`；`cube_mmad` 恰好出现 `{cube0}`；`mix_1_1` 恰好出现
  `{cube0,vector0}`；`mix_1_2` 恰好出现 `{cube0,vector0,vector1}`。同一核心可重复，
  但不得缺少或出现额外的 `sub_block_id`，且这些受约束 case 的每行 `block_id` 非空。
  `simt_hello` 不约束核心集合。
- `HardwareInfo.jsonl` 恰好包含 `Host Info`、`Device Info`、`CPU Information`、
  `AI Core Information`、`Memory Information` 五类记录，且设备架构为 `3510`。
- `--list-sections` 返回的每个 Section 都有非空 CSV；每个 CSV 必须符合严格 CSV 语法，
  至少包含表头和一行数据，表头名称非空且唯一，且所有数据行字段数与表头一致。不可用
  计数器对应的数据或指标单元格可以为空。
- `result.npu-rep` 是非空普通文件，工具输出的数据目录和报告路径与本次请求一致。

全量运行会继续执行全部六个 case，并逐项打印 `PASS` 或 `FAIL`。整体通过时最终汇总必须为：

```text
total=6 passed=6 failed=0
```

## 维护

日常功能修改不应改动固定 smoke 场景。确需更新样例、runner、测试或本文档时，应单独评审其
行为变化。

提交前执行布局校验：

```bash
bash npu_tools/npu_compute/smoke/tests/check_smoke_layout.sh
```
