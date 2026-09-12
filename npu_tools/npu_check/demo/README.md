# ACLSan Demo

该目录直接复用产品级 `npu_tools/npu_check/src/cli` 和 `npu_tools/npu_check/src/processor`。
`npu-check` 启动示例程序，通过 CANN API Injection 加载 `libnpu_check.so`，再按所选工具订阅并
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
| `examples/basic_func/dual_tool_multi_launch_aggregate` | `dual_tool_multi_launch_aggregate.asc` | `demo` | 同时启用 Memcheck 和 Synccheck 的双 launch 聚合示例。 |
| `examples/synccheck` | 每个场景一个独立目录 | `demo` | 同步指令配对的正常与异常样例。 |
| `examples/plog_check` | `plog_check.asc` | `plog_check` | 验证 npu-check 内部 INFO 日志写入 CANN Host plog。 |

`examples/synccheck` 中每个用例目录都包含独立的 `.asc`、CMake、runner 和结果验证器，
`verify_common.py` 提供公共校验能力；每个用例的 `run.sh` 在自身的 `build/` 中构建并运行
对应 `.asc`。`.asc` 包含完整的 AscendC kernel、
ACL 初始化、kernel launch 和清理逻辑。
保留套件平衡覆盖 SET_FLAG/WAIT_FLAG 与 GET_BUF/RLS_BUF 的正常配对、重复打开、未匹配关闭、
未消费打开、多 launch 聚合和多 block 隔离。`mix_wait_without_set` 与
`flag_set_set_wait_wait` 覆盖会阻塞的异常场景，使用 `aclrtSynchronizeStreamWithTimeout`
限制等待时间，并用 `aclrtDestroyStreamForce` 销毁仍包含阻塞 kernel 的 stream。

## 构建与产物

顶层 `build.sh` 加载 `${ASCEND_HOME_PATH}/set_env.sh`，在仓库根目录执行 `bash build.sh --pkg`，
再把生成的 `.run` 安装到当前 `ASCEND_HOME_PATH`。runner 使用的入口位于：

```text
${ASCEND_HOME_PATH}/<arch>-linux/bin/npu-check
```

每个示例目录都是完整的独立 CMake 工程。对应 `run.sh` 将其配置和构建到：

```text
npu_sanitizer/demo/examples/<分类>/<用例名>/build
```

示例可执行文件直接位于上述构建目录，例如：

```text
npu_sanitizer/demo/examples/memcheck/add/build/demo
```

`build.sh` 安装 `npu-check` 及其共享库。运行时首次加载 kernel binary 时生成并缓存
`probe.o` 和 `ctrl.bin`。每个用例将日志和其他运行文件保存在自身的 `build/` 中；例如
Synccheck 日志位于 `demo/examples/synccheck/<用例名>/build/npu_check.log`，Matmul 输入与输出位于
对应用例的 `build/run/`。

### 运行链使用的共享库

| 产物 | CMake target 或来源 | 源码来源 | 进入 demo 的方式 | 作用 |
| --- | --- | --- | --- | --- |
| `libnpu_check.so` | `npu_check` | `npu_tools/npu_check/src/processor/` | `npu_check` 将绝对路径写入 `ACL_API_INJECTION`；CANN 在 `aclInit()` 期间加载并调用 `acltoolInitialize()` | 建立 UDS 会话，订阅并启用所选工具的 callback，生成诊断和 summary |
| `libacl_san.so` | `acl_san` | `npu_tools/npu_check/src/acl_san/`，源文件由该目录的 `CMakeLists.txt` 明确列出 | `libnpu_check.so` 的 `DT_NEEDED` 依赖 | 提供 ACLSan 公共 API、callback 路由、cbdata 构造和 Runtime hook replacement |
| `libacl_tool_injection.so` | `acl_tool_injection` | `npu_compute/src/injection_hook/injection_hook.cpp` | `libacl_san.so` 的 `DT_NEEDED` 依赖 | 安装固定 trampoline，保存并切换 `orig/hook/custom` Runtime 函数入口 |
| `libacl_rt.so` | CANN 安装库 | CANN 9.2.0 安装包 | 所有示例及 `libacl_tool_injection.so` 的 `DT_NEEDED` 依赖 | 导出 `aclrt*` 和 `aclrtApiInjectionGetFunc/SetFunc`，再调用底层 RTS |
| `libruntime.so` | CANN Runtime | CANN 9.2.0 安装包 | `libacl_rt.so` 的 `DT_NEEDED` 依赖 | 提供底层 `rt*`/RTS 实现；不直接导出 `aclrtMalloc` |
| `libprofapi.so` | `CANN::profapi` | CANN 9.2.0 安装包 | `libacl_tool_injection.so` 和 `libruntime.so` 的 `DT_NEEDED` 依赖 | 提供 CANN profiling 与工具注入支持 |

根目录的打包流程会构建 asc-tools 正式包，不构建 test-only Runtime/Profiling stub。
若显式构建 `npu_compute` 的其他常规 target，还可生成
`libacl_pti.so`（target `acl_pti`）和 `libnpu-compute.so`（target `npu_compute`）；
它们不在本 demo 的运行链中。`libstdc++.so`、`libc.so`、`libdl.so` 等系统库也
不属于本仓库生成的 demo 产物。

除共享库外，包中还包含来自 target `npu_check_cli` 的 `npu-check`。各用例 runner
在独立构建目录中按需生成对应示例程序。包中的 `libnpu_check.so` 直接链接 `acl_san`，而 CLI
通过 `ACL_API_INJECTION` 指定 `libnpu_check.so`；demo 目录不再持有 `npu_check` 或
`npu_check_exec` 源码副本，也不生成旧 `npucheck` 二进制。

## 运行

在仓库根目录加载 CANN 环境，再统一构建工具：

```bash
source /usr/local/Ascend/cann/set_env.sh
bash ./npu_sanitizer/demo/build.sh
```

随后进入任一用例目录执行 `run.sh`：

```bash
bash ./npu_tools/npu_check/demo/examples/memcheck/add/run.sh
bash ./npu_tools/npu_check/demo/examples/plog_check/run.sh
bash ./npu_tools/npu_check/demo/examples/memcheck/matmul_basic_api/run.sh
bash ./npu_tools/npu_check/demo/examples/basic_func/padding_register_state/run.sh
bash ./npu_tools/npu_check/demo/examples/basic_func/dual_tool_multi_launch_aggregate/run.sh
bash ./npu_tools/npu_check/demo/examples/synccheck/multi_launch_pairs/run.sh
```

一次执行全部 18 个基础能力和 Synccheck 用例：

```bash
bash ./npu_sanitizer/demo/run_smoke.sh
```

`run_smoke.sh` 是全量冒烟的一键入口，无需预先执行 `build.sh`。它先通过 `build.sh` 打包并安装
asc-tools，再按固定顺序执行所有 case。每个 case 的
控制台输出保存到 `demo/build/smoke/<分类>/<用例名>.log`。打包或安装失败时脚本立即退出；
任一用例验证失败时，脚本继续执行其余用例，最后返回 1。

一次运行全部 Synccheck 用例：

```bash
bash ./npu_sanitizer/demo/examples/synccheck/run_all.sh
```

顶层 `build.sh` 要求 `ASCEND_HOME_PATH` 已设置，并加载 `${ASCEND_HOME_PATH}/set_env.sh`。
由于 `.run` 安装器固定写入 `<install-path>/cann`，当目标目录名不是 `cann` 时，脚本会在同级创建
临时 `cann` 软链接。路径被其他文件、目录或不同目标的软链接占用时立即报错；退出时只删除本次
创建且仍指向目标 CANN 的软链接，不删除 CANN 目录或其中任何内容。各用例 `run.sh` 要求当前 shell 已通过 `set_env.sh` 设置
`ASCEND_HOME_PATH` 和 CANN 工具链环境。若 CANN 安装位置不同，可在运行前加载对应环境：

```bash
source /path/to/cann/set_env.sh
bash ./npu_sanitizer/demo/build.sh
```

每个 `run.sh` 在 `demo/examples/<分类>/<用例名>/build` 中独立配置并构建自己的 target，
再按分类选择 `memcheck`、`synccheck` 或同时启用两者。
Matmul runner 还会生成输入和 golden 数据，并执行 `verify_result.py`。成功时会输出
`test pass!`。所有 runner 都会校验结果、应用退出状态、UDS handshake、sanitizer summary 和
session end。预期诊断已完整校验的用例返回 0，最后打印
`example verification passed: <分类>/<用例名>`；日志不符时返回非零。

`padding_register_state` 使用单 cube block kernel 依次执行 `asc_set_l13d_padding(0x12)` 和
`asc_set_l13d_padding(0x34)`，并验收应用结果、零错误 summary 与完整 session。寄存器状态键和
最新值语义由 `register_state_manager_test` 独立验证，不作为客户可见 demo 的判定条件。
`dual_tool_multi_launch_aggregate` 在同一 stream 上先触发 GM 越界读，再留下未消费的
`SET_FLAG`，并只同步一次，验证 Memcheck 和 Synccheck 会共同结算两个 launch。

## 运行链路

```text
npu-check
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
`npu_tools/npu_check/src/processor/tool_manager/entry.cpp`。

## 验证范围

- `examples/memcheck/memory_access/check_memory_access_end_to_end.sh` 运行 244 个支持的
  memory_access 场景，只校验客户可见诊断、memcheck summary、CLI 结果和会话完整性。
- NDDMA 新增的同 block stride 覆盖和合法边界用例见
  [NDDMA 状态与边界验证](examples/memcheck/memory_access/NDDMA_STATE_CASES.md)。

当前 CANN/Device 环境中，三个示例均已通过真实 Device E2E 验证。demo 和冒烟只以客户可见的
诊断、summary、CLI 状态、完整会话以及应用计算结果作为通过条件，不依赖内部 trace 日志。
这些结果不外推到下述尚未覆盖场景。

## 当前限制与 TODO

示例编译需要 CANN 9.2.0 的 ASC 编译器；实际运行还需要可用 Ascend Device 及匹配 Driver。
没有可用设备时，`aclInit` 可能失败，这不表示示例的 CMake 配置或二进制链接失败。

仍有以下未完成项：

- 当前基础 E2E 只覆盖三个示例实际触发的 CCE 指令；更完整的受支持指令覆盖由 GM 搬运矩阵扩展，
  新增通路仍需客户可见诊断断言和真实 Device E2E。
- 尚未验证并发、callback 重入和共享库卸载安全；若这些属于产品支持范围，需要增加相应的
  生命周期与压力测试。

内部 API、CLI 维测及 DBI 诊断统一写入 CANN Host plog，由 `ASCEND_GLOBAL_LOG_LEVEL` 控制等级；需要详细信息时设为 `0`。设置 `ASCEND_SLOG_PRINT_TO_STDOUT=0` 可避免 CANN 将内部日志打印到终端。自定义内部文件 Logger 和日志开关已移除。样例的 `build/npu_check.log` 是脚本保存的 check／应用输出，不是内部日志。
