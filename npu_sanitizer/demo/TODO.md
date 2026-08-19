# sanitizer_api demo 说明

## 构建产物映射

`run.sh` 先删除 `npu_sanitizer/demo/build`，再由
`npu_sanitizer/demo/CMakeLists.txt` 把 `npu_compute`、`sanitizer_api` 和 demo
自身加入同一棵 CMake 构建树。仓库内生成的共享库统一落到：

```text
npu_sanitizer/demo/build/npu_compute/bin
```

### demo 运行链实际使用的 `.so`

| 产物 | CMake target | 源码来源 | 如何进入 demo | 作用 |
| --- | --- | --- | --- | --- |
| `libnpu_check.so` | `npu_check` | `npu_sanitizer/demo/npu_check/subscribe.cpp`，目标定义在 `npu_sanitizer/demo/npu_check/CMakeLists.txt` | `npucheck` 把库名写入 `ACL_API_INJECTION`；`aclrtInit()` 调用 `libprofapi.so` 后，由后者执行 `dlopen()` | 实现 `acltoolInitialize()`，订阅并启用 memcheck、synccheck callback |
| `libacl_san.so` | `aclsan_api`，通过 `OUTPUT_NAME acl_san` 改名 | `npu_sanitizer/sanitizer_api/src/*.cpp` 中由 `npu_sanitizer/sanitizer_api/CMakeLists.txt` 明确列入 `aclsan_api` 的源文件 | `libnpu_check.so` 的 `DT_NEEDED` 依赖 | 提供 ACLSan 公共 API、callback 路由、cbdata 构造和 Runtime hook replacement |
| `libacl_tool_injection.so` | `acl_tool_injection` | `npu_compute/src/injection_hook/injection_hook.cpp`，目标定义在同目录的 `CMakeLists.txt` | `libacl_san.so` 的 `DT_NEEDED` 依赖 | 安装固定 trampoline，保存并切换 `orig/hook/custom` Runtime 函数入口 |
| `libruntime.so` | `acl_runtime_stub`，通过 `OUTPUT_NAME runtime` 改名 | `npu_compute/stubs/runtime/runtime_stub.cpp`，目标定义在同目录的 `CMakeLists.txt` | `aclsan_demo_app` 的 `DT_NEEDED` 依赖；`libacl_tool_injection.so` 也链接该库 | 提供 Host 侧 `aclrt*` stub 和可替换的 Runtime 分发表，不是真实 CANN Runtime |
| `libprofapi.so` | `acl_prof_api_stub`，通过 `OUTPUT_NAME profapi` 改名 | `npu_compute/stubs/prof_api/prof_api_stub.cpp`，目标定义在同目录的 `CMakeLists.txt` | `libruntime.so` 和 `libacl_tool_injection.so` 的 `DT_NEEDED` 依赖 | 提供 Host 侧 profiling stub；读取 `ACL_API_INJECTION`、`dlopen(libnpu_check.so)` 并调用 `acltoolInitialize()` |

### 同一构建树生成、但本 demo 运行链不使用的 `.so`

顶层 `cmake --build` 默认还会构建 `npu_compute` 的常规库。它们与上述库位于同一输出目录，但 `npucheck -> aclsan_demo_app` 这条 Host-stub 链路不加载它们：

| 产物 | CMake target | 源码来源 |
| --- | --- | --- |
| `libacl_pti.so` | `acl_pti` | `npu_compute/src/acl_pti/`，源文件清单在该目录的 `CMakeLists.txt` |
| `libnpu-compute.so` | `npu_compute`，通过 `OUTPUT_NAME npu-compute` 改名 | `npu_compute/src/npu_compute/`，源文件清单在该目录的 `CMakeLists.txt` |

这里的“每个 `.so`”仅统计该仓库在 demo 构建目录中生成的共享库；`libstdc++.so`、`libc.so`、`libdl.so` 等编译器或系统运行库由系统工具链提供，不是本 demo 的源码产物。

另外两个产物不是 `.so`：`npucheck` 来自 target `npucheck` 和
`npu_sanitizer/demo/npu_check_exec/main.cpp`；`aclsan_demo_app` 来自 target
`aclsan_demo_app` 和 `npu_sanitizer/demo/demo_app.cpp`。

demo 中的 `npu_check` CMake 在同一个构建树内直接链接 `aclsan_api`，因此不需要在 CMake 配置阶段预先生成 `libacl_san.so`；独立构建 `npu_check` 时仍会检查该库文件是否存在。
日志统一引用 `npu_sanitizer/sanitizer_api/include/internal/aclsan_log.h`，demo 不再依赖旧的
`npu_check/common` 日志目录。

## 运行链路

```text
npucheck
  -> 设置 ACL_API_INJECTION=libnpu_check.so 后启动 aclsan_demo_app
  -> aclsan_demo_app 调用 libruntime.so 的 aclrtInit()
  -> libruntime.so 调用 libprofapi.so 的 ProfApiLoadApiInjectionFromEnv()
  -> libprofapi.so 执行 dlopen(libnpu_check.so) 并调用 acltoolInitialize()
  -> libnpu_check.so 调用 libacl_san.so 完成 Subscribe/Enable
  -> libacl_san.so 通过 libacl_tool_injection.so 注册固定 trampoline 和 hook
  -> 后续 aclrt* 调用进入 hook，再分发给 memcheck、synccheck callback
```

运行命令：

```bash
bash /home/cty/asc-tools-aclsan/npu_sanitizer/demo/run.sh
```

端到端检查脚本是 `tests/check_end_to_end.sh`，会验证 callback 输出、子进程退出状态和关键 ELF `DT_NEEDED` 关系。

## 当前限制

- 当前使用 `npu_compute` 的 Host runtime stub 和 injection hook，只验证 Host 调用链、动态库依赖、Subscribe/Enable 和 callback 分发；不代表真实 CANN/Device 行为已经验证。
- 已删除未参与实际加载链路的 `demo_tool.cpp` 和 `libaclsan_demo_tool.so` target；`acltoolInitialize` 唯一来源为
  `npu_sanitizer/demo/npu_check/subscribe.cpp`。
