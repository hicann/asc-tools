# ACLSan Demo

该目录直接复用产品级 `npu_sanitizer/npu_check_cli` 和 `npu_sanitizer/npu_check`。
`npu_check` 启动示例程序，通过 CANN API Injection 加载 `libnpu_check.so`，再订阅并启用
`libacl_san.so` 的 memcheck callback。

## 示例目录

| 目录 | 源文件 | 生成的可执行文件 | 说明 |
| --- | --- | --- | --- |
| `examples/add` | `add.asc` | `aclsan_demo_add` | AscendC 向量加法示例，校验计算结果。 |
| `examples/matmul_basic_api` | `matmul_basic_api.asc` | `aclsan_demo_matmul_basic_api` | 基础矩阵乘样例。 |
| `examples/matmul_leakyrelu_basic_api` | `matmul_leakyrelu_basic_api.asc` | `aclsan_demo_matmul_leakyrelu_basic_api` | Matmul 与 LeakyRelu 融合样例。 |

## 构建与产物

`run.sh` 每次运行先删除固定目录 `npu_sanitizer/demo/build`，再由顶层
`CMakeLists.txt` 把 `npu_compute`、`sanitizer_api`、`npu_sanitizer/common`、
`npu_sanitizer/npu_check_cli`、`npu_sanitizer/npu_check` 和 `examples/add` 加入
同一棵 CMake 构建树。该构建树的工具和 add 产物统一位于：

```text
npu_sanitizer/demo/build/npu_compute/bin
```

两个 matmul 示例使用各自目录中的独立 CMake 构建树，其可执行文件分别位于
`examples/matmul_basic_api/build/bin` 和
`examples/matmul_leakyrelu_basic_api/build/bin`。

### 运行链使用的共享库

| 产物 | CMake target 或来源 | 源码来源 | 进入 demo 的方式 | 作用 |
| --- | --- | --- | --- | --- |
| `libnpu_check.so` | `npu_check` | `npu_sanitizer/npu_check/src/` | `npu_check` 将绝对路径写入 `ACL_API_INJECTION`；CANN 在 `aclInit()` 期间加载并调用 `acltoolInitialize()` | 建立 UDS 会话，订阅并启用 memcheck callback，生成诊断和 summary |
| `libacl_san.so` | `acl_san` | `npu_sanitizer/sanitizer_api/`，源文件由该目录的 `CMakeLists.txt` 明确列出 | `libnpu_check.so` 的 `DT_NEEDED` 依赖 | 提供 ACLSan 公共 API、callback 路由、cbdata 构造和 Runtime hook replacement |
| `libacl_tool_injection.so` | `acl_tool_injection` | `npu_compute/src/injection_hook/injection_hook.cpp` | `libacl_san.so` 的 `DT_NEEDED` 依赖 | 安装固定 trampoline，保存并切换 `orig/hook/custom` Runtime 函数入口 |
| `libacl_rt.so` | `CANN::acl_rt` | CANN 9.2.0 安装包 | 三个示例及 `libacl_tool_injection.so` 的 `DT_NEEDED` 依赖 | 导出 `aclrt*` 和 `aclrtApiInjectionGetFunc/SetFunc`，再调用底层 RTS |
| `libruntime.so` | CANN Runtime | CANN 9.2.0 安装包 | `libacl_rt.so` 的 `DT_NEEDED` 依赖 | 提供底层 `rt*`/RTS 实现；不直接导出 `aclrtMalloc` |
| `libprofapi.so` | `CANN::profapi` | CANN 9.2.0 安装包 | `libacl_tool_injection.so` 和 `libruntime.so` 的 `DT_NEEDED` 依赖 | 提供 CANN profiling 与工具注入支持 |

`run.sh` 只构建 `npu_check_cli`、所选示例及其依赖，不构建 test-only
Runtime/Profiling stub。若显式构建 `npu_compute` 的其他常规 target，还可生成
`libacl_pti.so`（target `acl_pti`）和 `libnpu-compute.so`（target `npu_compute`）；
它们不在本 demo 的运行链中。`libstdc++.so`、`libc.so`、`libdl.so` 等系统库也
不属于本仓库生成的 demo 产物。

除共享库外，顶层构建树还生成来自 target `npu_check_cli` 的 `npu_check` 和
`aclsan_demo_add`。同一构建树中的 `libnpu_check.so` 直接链接 `acl_san`，而 CLI
通过 `ACL_API_INJECTION` 指定 `libnpu_check.so`；demo 目录不再持有 `npu_check` 或
`npu_check_exec` 源码副本，也不生成旧 `npucheck` 二进制。

## 运行

在本目录或任意工作目录执行下列命令：

```bash
bash /home/cty/asc-tools-aclsan/npu_sanitizer/demo/run.sh
```

无参数时运行 `examples/add`。显式选择示例：

```bash
bash /home/cty/asc-tools-aclsan/npu_sanitizer/demo/run.sh add
bash /home/cty/asc-tools-aclsan/npu_sanitizer/demo/run.sh matmul_basic_api
bash /home/cty/asc-tools-aclsan/npu_sanitizer/demo/run.sh matmul_leakyrelu_basic_api
```

`run.sh` 每次运行都会删除并重建固定目录 `npu_sanitizer/demo/build`，自动加载
`${NPUCOMPUTE_CANN_ROOT}/../set_env.sh`；默认的 `NPUCOMPUTE_CANN_ROOT` 为
`/home/cty/cann_0819/cann-9.2.0/x86_64-linux`。若 CANN 安装位置不同，可在运行前覆盖：

```bash
NPUCOMPUTE_CANN_ROOT=/path/to/cann/x86_64-linux \
    bash /home/cty/asc-tools-aclsan/npu_sanitizer/demo/run.sh add
```

构建完成后，`npu_check` 会以 `memcheck` 启动所选示例。两个 matmul 目录中的
`run.sh` 各自清理并重建本目录的 `build`、生成输入和 golden 数据，顶层 `run.sh`
再从对应的 `build/run` 目录调用可执行文件并执行 `verify_result.py`。成功时会输出
`test pass!`，并以结果校验、应用退出状态、UDS handshake、sanitizer summary 和
session end 共同决定执行结果。

## 运行链路

```text
npu_check
  -> 创建私有 UDS 会话，并设置 ACL_API_INJECTION=libnpu_check.so 的绝对路径
  -> 启动所选示例
  -> 示例调用 libacl_rt.so 的 aclInit()
  -> CANN profiling/injection 机制加载 libnpu_check.so，并调用 acltoolInitialize()
  -> libnpu_check.so 从 UDS 接收 memcheck 配置，并调用 libacl_san.so 完成 Subscribe/Enable
  -> libacl_san.so 通过 libacl_tool_injection.so 调用 aclrtApiInjectionGetFunc/SetFunc
  -> 后续 aclrt* 调用进入 hook；replacement 调用 original aclrt*，最终进入 libruntime.so 的 RTS 实现
  -> callback 数据交给产品级 memcheck，诊断、summary 和 session end 经 UDS 返回 CLI
```

`acltoolInitialize` 的唯一源码定义位于
`npu_sanitizer/npu_check/src/tool_manager/entry.cpp`。

## 验证范围

- `tests/check_product_npu_check_layout.sh` 检查产品级 target、入口符号拼写以及
  demo 不再引用旧 launcher 或不支持的工具。
- `tests/check_examples_layout.sh` 检查三个示例的目录和 runner 契约，并确认已删除的
  `examples/test` 未重新进入构建或运行链。
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

- 产品级 CLI 和 `ToolManager` 当前只接受单个 `--tool memcheck`。`report_renderer` 虽然
  已有 `synccheck` 报告格式支持，但 `synccheck` 尚未接入可执行工具链，demo 也不传递或
  验证 `synccheck`。
- 当前 probe E2E 只覆盖三个示例实际触发的 CCE 指令；若要声明更完整的指令覆盖，需要
  增加其他 CCE 指令的用例、record 解析断言和真实 Device E2E。
- 尚未验证并发、callback 重入和共享库卸载安全；若这些属于产品支持范围，需要增加相应的
  生命周期与压力测试。
