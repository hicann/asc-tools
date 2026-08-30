# ACLSan Demo

该目录直接复用产品级 `npu_sanitizer/npu_check_cli` 和 `npu_sanitizer/npu_check`。
`npu_check` 启动示例程序，通过 CANN API Injection 加载 `libnpu_check.so`，再按所选工具订阅并
启用 `libacl_san.so` 的 callback。

## 示例目录

| 目录 | 源文件 | 生成的可执行文件 | 说明 |
| --- | --- | --- | --- |
| `examples/memcheck/add` | `add.asc` | `demo` | AscendC 向量加法及基础 GM 访问示例。 |
| `examples/memcheck/datacopy_stride` | `datacopy_stride.asc` | `demo` | DataCopy stride 越界检查示例。 |
| `examples/memcheck/matmul_basic_api` | `matmul_basic_api.asc` | `demo` | 基础矩阵乘样例。 |
| `examples/memcheck/matmul_leakyrelu_basic_api` | `matmul_leakyrelu_basic_api.asc` | `demo` | Matmul 与 LeakyRelu 融合样例。 |
| `examples/basic_func/multi_kernel` | `multi_kernel.asc` | `demo` | 多 kernel 加载与插桩基础能力示例。 |
| `examples/basic_func/padding_register_state` | `padding_register_state.asc` | `demo` | `SET_PADDING` register manager 基础能力示例。 |
| `examples/synccheck` | 每个场景一个独立目录 | `demo` | 同步指令配对的正常与异常样例。 |

`examples/synccheck` 中每个用例目录都包含独立的 `.asc`、CMake、runner 和结果验证器，
`verify_common.py` 提供公共校验能力；每个用例的 `run.sh` 从统一的 `demo/build` 构建并运行
对应 `.asc`。`.asc` 包含完整的 AscendC kernel、
ACL 初始化、kernel launch 和清理逻辑。
`wait_without_set` 会故意执行没有对应
SET_FLAG 的 WAIT_FLAG。目录同时使用 `AscendC::Mutex::Lock/Unlock` 覆盖 GET_BUF/RLS_BUF 的
正常与异常配对，以及 SET/WAIT 和 GET/RLS 的多 block 隔离。`wait_without_set` 和重复 Lock
场景必须用 `aclrtSynchronizeStreamWithTimeout` 限制等待时间，并用 `aclrtDestroyStreamForce`
销毁仍包含阻塞 kernel 的 stream，才能正常结束进程。

## 构建与产物

顶层 `build.sh` 删除并重新配置固定目录 `npu_sanitizer/demo/build`，由顶层 `CMakeLists.txt`
构建产品工具、共享库和在线 DBI 源码，不注册示例 target。公共工具位于：

```text
npu_sanitizer/demo/build/npu_compute/bin
```

每个示例目录都是完整的独立 CMake 工程。对应 `run.sh` 将其配置和构建到：

```text
npu_sanitizer/demo/examples/<分类>/<用例名>/build
```

示例可执行文件直接位于上述构建目录，例如：

```text
npu_sanitizer/demo/examples/memcheck/add/build/demo
```

`build.sh` 同时准备在线 DBI 所需的 `dbi_runtime_sources`。运行时首次加载 kernel binary 时生成并
缓存 `probe.o` 和 `ctrl.bin`。每个用例将日志和其他运行文件保存在自身的 `build/` 中；例如
Synccheck 日志位于 `demo/examples/synccheck/<用例名>/build/npu_check.log`，Matmul 输入与输出位于
对应用例的 `build/run/`。

### 运行链使用的共享库

| 产物 | CMake target 或来源 | 源码来源 | 进入 demo 的方式 | 作用 |
| --- | --- | --- | --- | --- |
| `libnpu_check.so` | `npu_check` | `npu_sanitizer/npu_check/src/` | `npu_check` 将绝对路径写入 `ACL_API_INJECTION`；CANN 在 `aclInit()` 期间加载并调用 `acltoolInitialize()` | 建立 UDS 会话，订阅并启用所选工具的 callback，生成诊断和 summary |
| `libacl_san.so` | `acl_san` | `npu_sanitizer/sanitizer_api/`，源文件由该目录的 `CMakeLists.txt` 明确列出 | `libnpu_check.so` 的 `DT_NEEDED` 依赖 | 提供 ACLSan 公共 API、callback 路由、cbdata 构造和 Runtime hook replacement |
| `libacl_tool_injection.so` | `acl_tool_injection` | `npu_compute/src/injection_hook/injection_hook.cpp` | `libacl_san.so` 的 `DT_NEEDED` 依赖 | 安装固定 trampoline，保存并切换 `orig/hook/custom` Runtime 函数入口 |
| `libacl_rt.so` | CANN 安装库 | CANN 9.2.0 安装包 | 所有示例及 `libacl_tool_injection.so` 的 `DT_NEEDED` 依赖 | 导出 `aclrt*` 和 `aclrtApiInjectionGetFunc/SetFunc`，再调用底层 RTS |
| `libruntime.so` | CANN Runtime | CANN 9.2.0 安装包 | `libacl_rt.so` 的 `DT_NEEDED` 依赖 | 提供底层 `rt*`/RTS 实现；不直接导出 `aclrtMalloc` |
| `libprofapi.so` | `CANN::profapi` | CANN 9.2.0 安装包 | `libacl_tool_injection.so` 和 `libruntime.so` 的 `DT_NEEDED` 依赖 | 提供 CANN profiling 与工具注入支持 |

`build.sh` 只构建 `npu_check_cli` 及其依赖，不构建 test-only
Runtime/Profiling stub。若显式构建 `npu_compute` 的其他常规 target，还可生成
`libacl_pti.so`（target `acl_pti`）和 `libnpu-compute.so`（target `npu_compute`）；
它们不在本 demo 的运行链中。`libstdc++.so`、`libc.so`、`libdl.so` 等系统库也
不属于本仓库生成的 demo 产物。

除共享库外，顶层构建树还生成来自 target `npu_check_cli` 的 `npu_check`。各用例 runner
在独立构建目录中按需生成对应示例程序。公共构建树中的 `libnpu_check.so` 直接链接 `acl_san`，而 CLI
通过 `ACL_API_INJECTION` 指定 `libnpu_check.so`；demo 目录不再持有 `npu_check` 或
`npu_check_exec` 源码副本，也不生成旧 `npucheck` 二进制。

## 运行

在仓库根目录加载 CANN 环境，再统一构建工具：

```bash
source /home/cty/cann_0829/cann/set_env.sh
bash ./npu_sanitizer/demo/build.sh
```

随后进入任一用例目录执行 `run.sh`：

```bash
bash ./npu_sanitizer/demo/examples/memcheck/add/run.sh
bash ./npu_sanitizer/demo/examples/memcheck/matmul_basic_api/run.sh
bash ./npu_sanitizer/demo/examples/basic_func/padding_register_state/run.sh
bash ./npu_sanitizer/demo/examples/synccheck/single_pair/run.sh
```

一次执行全部 21 个基础能力和 Synccheck 用例：

```bash
bash ./npu_sanitizer/demo/run_smoke.sh
```

`run_smoke.sh` 是全量冒烟的一键入口，无需预先执行 `build.sh`。它先删除 `demo/build`，再通过
`build.sh` 重新构建公共工具并加载 CANN 环境，随后按固定顺序执行所有 case。每个 case 的
控制台输出保存到 `demo/build/smoke/<分类>/<用例名>.log`。公共工具构建失败时脚本立即退出；
任一用例验证失败时，脚本继续执行其余用例，最后返回 1。

一次运行全部 Synccheck 用例：

```bash
bash ./npu_sanitizer/demo/examples/synccheck/run_all.sh
```

顶层 `build.sh` 会加载 `${ASCEND_HOME_PATH}/set_env.sh`，未设置时默认使用
`/home/cty/cann_0829/cann`。各用例 `run.sh` 要求当前 shell 已通过 `set_env.sh` 设置
`ASCEND_HOME_PATH` 和 CANN 工具链环境。若 CANN 安装位置不同，可在运行前加载对应环境：

```bash
source /path/to/cann/set_env.sh
bash ./npu_sanitizer/demo/build.sh
```

每个 `run.sh` 在 `demo/examples/<分类>/<用例名>/build` 中独立配置并构建自己的 target，
再按分类选择 `memcheck` 或 `synccheck`。
Matmul runner 还会生成输入和 golden 数据，并执行 `verify_result.py`。成功时会输出
`test pass!`。所有 runner 都会校验结果、应用退出状态、UDS handshake、sanitizer summary 和
session end。预期诊断已完整校验的用例返回 0，最后打印
`example verification passed: <分类>/<用例名>`；日志不符时返回非零。

`padding_register_state` 使用单 cube block kernel 依次执行 `asc_set_l13d_padding(0x12)` 和
`asc_set_l13d_padding(0x34)`。Device 日志证明值为 `0x12` 和 `0x34` 的两条 `SET_PADDING` raw
record 被解码并交给同一个 register-state key；`register_state_manager_test` 通过 `Get()` 独立验证
同一 key 只保存最新值。

## 运行链路

```text
npu_check
  -> 创建私有 UDS 会话，并设置 ACL_API_INJECTION=libnpu_check.so 的绝对路径
  -> 启动所选示例
  -> 示例调用 libacl_rt.so 的 aclInit()
  -> CANN profiling/injection 机制加载 libnpu_check.so，并调用 acltoolInitialize()
  -> libnpu_check.so 从 UDS 接收工具配置，并调用 libacl_san.so 完成 Subscribe/Enable
  -> libacl_san.so 通过 libacl_tool_injection.so 调用 aclrtApiInjectionGetFunc/SetFunc
  -> binary load hook 按已启用 callback 在线编译对应 probe，链接并插桩 kernel binary
  -> 后续 aclrt* 调用进入 hook；replacement 调用 original aclrt*，最终进入 libruntime.so 的 RTS 实现
  -> callback 数据交给所选 checker，诊断、summary 和 session end 经 UDS 返回 CLI
```

`acltoolInitialize` 的唯一源码定义位于
`npu_sanitizer/npu_check/src/tool_manager/entry.cpp`。

## 验证范围

- `tests/check_product_npu_check_layout.sh` 检查产品级 target、入口符号拼写以及
  demo 不再引用旧 launcher 或不支持的工具。
- `tests/check_examples_layout.sh` 检查所有示例分类、runner 自校验契约和 `run_smoke.sh` 的
  汇总行为，并确认已删除的 `examples/test` 未重新进入构建或运行链。
- `tests/check_end_to_end.sh` 在真实 Device 上运行 add，检查 UDS 生命周期、应用退出状态、
  `acltoolInitialize@@NPU_CHECK_1.0` 导出和关键 ELF `DT_NEEDED` 关系。
- `tests/check_probe_device_end_to_end.sh` 检查 add 的 probe record 回读；
  `tests/check_matmul_end_to_end.sh` 依次检查两个 matmul 的 record 数量、AIV block 覆盖和
  计算结果。

当前 CANN/Device 环境中，三个示例均已验证 Device probe 插桩、record 回读、
memcheck 分析和计算结果：add、基础 matmul、融合 matmul 分别回读 24、12、36 条 record。
这属于真实 Device E2E 证据，但不外推到下述尚未覆盖场景。

## 当前限制与 TODO

示例编译需要 CANN 9.2.0 的 ASC 编译器；实际运行还需要可用 Ascend Device 及匹配 Driver。
没有可用设备时，`aclInit` 可能失败，这不表示示例的 CMake 配置或二进制链接失败。

仍有以下未完成项：

- 当前 probe E2E 只覆盖三个示例实际触发的 CCE 指令；若要声明更完整的指令覆盖，需要
  增加其他 CCE 指令的用例、record 解析断言和真实 Device E2E。
- 尚未验证并发、callback 重入和共享库卸载安全；若这些属于产品支持范围，需要增加相应的
  生命周期与压力测试。
