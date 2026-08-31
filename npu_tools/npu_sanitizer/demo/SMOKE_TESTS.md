# Demo 冒烟用例

本文档说明 `npu_tools/npu_sanitizer/demo/examples` 下样例的最小冒烟方式，用于快速确认样例编译、
`npu_check` 启动、Device 执行和结果验证链路是否可用。所有命令均在仓库根目录执行。

## 环境准备

样例默认使用 `/home/cty/cann_0829/cann`。CANN 安装位置不同时，通过环境变量覆盖：

```bash
export ASCEND_HOME_PATH=/path/to/cann
```

实际运行还需要可用的 Ascend Device，以及与 CANN 匹配的 Driver。
运行任一用例前先统一构建工具和共享库：

```bash
bash npu_tools/npu_sanitizer/demo/build.sh
```

## 全量冒烟

```bash
bash npu_tools/npu_sanitizer/demo/run_smoke.sh
```

该脚本无需预先执行 `build.sh`。它先删除 `demo/build`，再调用 `build.sh` 重新构建公共工具并
加载 CANN 环境，随后按固定顺序运行全部 21 个用例。公共工具构建失败时立即退出；用例中途
失败不会中断其余用例。每个用例输出保存到 `demo/build/smoke/<分类>/<用例名>.log`，最终以
`total/passed/failed` 汇总并在任一用例失败时返回 1。

## Add

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/add/run.sh
```

该用例通过 `memcheck` 运行向量加法，并故意触发一条 8256 字节的 GM 越界读。runner 校验原始
`npu_check` 状态为 2、summary 的 `errors=1` 和完整会话后返回 0。

## DataCopy Stride

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/datacopy_stride/run.sh
```

该用例故意触发 4 条 32 字节 GM 越界读，并校验 stride 参数字段、summary 的 `errors=4` 和完整会话。

## 基础能力用例

```bash
bash npu_tools/npu_sanitizer/demo/examples/basic_func/multi_kernel/run.sh
bash npu_tools/npu_sanitizer/demo/examples/basic_func/padding_register_state/run.sh
```

`multi_kernel` 校验两个 kernel 各一条 32 字节越界读以及不同的 launchId；
`padding_register_state` 校验无错误 summary、同一 register-state key 和 `0x12`/`0x34` 的
`SET_PADDING` 状态传递。

## Matmul Basic API

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/matmul_basic_api/run.sh
```

脚本编译基础矩阵乘样例、生成输入和 golden 数据，再通过 `memcheck` 运行。除工具会话需要
正常结束外，结果验证程序还应输出 `test pass!`。

## Matmul LeakyReLU Basic API

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/matmul_leakyrelu_basic_api/run.sh
```

该样例覆盖 Cube 与 Vector 协同的 Matmul 和 LeakyReLU 计算，通过 `memcheck` 运行，并使用
golden 数据验证最终输出。结果验证程序应输出 `test pass!`。

## Synccheck 冒烟用例

synccheck 提供独立的冒烟套件。一次运行全部用例并统计结果：

```bash
bash npu_tools/npu_sanitizer/demo/examples/synccheck/run_all.sh
```

单独运行某个用例时，直接运行用例目录中的 `run.sh`：

```bash
bash npu_tools/npu_sanitizer/demo/examples/synccheck/single_pair/run.sh
```

正常用例的原始 `npu_check` 状态为 0；异常用例用非零原始状态验证指定同步错误被检出。两类
runner 都在 `verify.py`、summary 和完整会话通过后返回 0，并输出
`example verification passed: synccheck/<用例名>`；`run_all.sh` 和 `run_smoke.sh` 据此统计
`PASS/FAIL`。

完整场景矩阵、环境变量和通过标准见
[synccheck/README.md](examples/synccheck/README.md)。

## 结果判定

- runner 构建失败、应用异常退出、工具握手失败、原始状态与预期不符或会话未完整结束，均视为失败。
- `add`、`datacopy_stride` 和 `multi_kernel` 是预期 memcheck 异常；每个 runner 校验其固定错误数量。
  两个 matmul 和 `padding_register_state` 的 checker summary 必须为 `errors=0`；matmul 还必须通过
  golden 数据校验。
- synccheck 的预期异常不是冒烟失败；只有诊断或 summary 与该用例的 `verify.py` 预期不符时才失败。
