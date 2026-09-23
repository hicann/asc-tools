# asc-tools 安装探测：本 Skill 辅助脚本

检查 CANN 安装的版本、组件与工具入口，帮助解决安装位置不明、环境混用或调用失败。探测结果用于选择可用命令，也可关联后续执行与解析记录；已有明确可用的工具路径时直接调用相应工具。CPU 工程的运行方法见下方 [CPU Debug](#cpu-debug)。

## 使用与输入

下文 `<artifact-dir>` 使用[任务产物目录](../../references/diagnosis/reporting.md#任务产物目录)的绝对路径，各输出均放在其内部。

```text
python "<skill-root>/scripts/debug/asc_tools_probe.py" \
  --expected-cann <version> --npu-arch <arch> --require <capability> \
  --output "<artifact-dir>/probe.json"
```

capability 为 `cpu-debug`、`npu-check`、`msobjdump` 或 `show-kernel-debug-data`。上面的示例要求版本和指定能力匹配；只想查看当前安装时可省略 `--expected-cann` 和 `--require`。已知用户安装根时增加 `--cann-root <path>`，探测限定在该根中。CPU Debug/npu check 需要目标 NPU Arch；CLI 能力可以省略架构。不清楚产品、架构号或目标宏对应关系时查 `/npu-arch`，参数值仍应与实际运行端和构建目标核对。当前参数见[实现](../debug/asc_tools_probe.py)及 `--help`。

CPU 库检查支持 `tools/cpudebug/{include,lib64,cmake}` 与 `tools/tikicpulib/lib/` 两种已知布局，优先尝试前者，再检查后者。每次检查使用同一布局中的头文件、库和 CMake 配置，不把两套不完整组件拼成“可用”。目标库导入不要求本机已有可运行的 NPU。

## 输出怎样解释

| 结果 | 实际含义 |
| --- | --- |
| CPU Debug `available` | 目标架构的 CPU 库依赖可被临时 CMake 工程导入；未验证 bisheng/ASC 工程完整编译，也未执行本次 Kernel |
| npu check `available` | CPU 库导入、插桩库与报告入口检查通过；未验证本次检查覆盖或生成日志 |
| CLI 就绪 | 找到入口并通过只读帮助调用；不证明任意输入均能解析 |
| 返回 `0 / 1 / 2` | 有效探测且 requirements 满足 / 有效探测但强制能力未满足 / 调用或报告无效 |

没有指定 `--require` 时，发现工具缺失仍可以返回 0，因为这是一份有效环境观察。`available`、`partial`、不可用状态只用于选择下一观察方式，不能推导 Ascend C API 的产品支持或当前数值结果。

## 继续使用

CPU 工程的运行与断点操作见下方说明，其他工具见[脚本索引](../README.md)。需要 API 文档或示例代码时，使用 `/ascendc-docs-search` skill。需要自动留证时用[证据执行器](run-with-evidence.md)运行当前工程命令；需要把报告用于适配器绑定时，保留示例中的 `--expected-cann` 和对应 `--require`。已有日志可直接交给相应解析器。

## CPU Debug

CPU Debug 将 Ascend C Kernel 编译为 CPU 域程序，适合先检查基本功能、地址计算和控制流，再用 printf/gdb 观察中间值。选择构建入口时先看工程采用哪种方式。[9.0.0 说明][cpu-source-1]、[9.1.0 说明][cpu-source-2]

### ASC 工程与三尖括号调用

适用于使用 ASC CMake 支持、通过 `<<<>>>` 调用 Kernel 的工程。9.1.0 的官方样例使用 `find_package(ASC REQUIRED)`、`project(... LANGUAGES ASC CXX)`，由 bisheng 处理 CPU 模式。在核函数调用所在源文件中加入：[样例 CMake][cpu-source-13]

```cpp
#ifdef ASCENDC_CPU_DEBUG
#include "cpu_debug_launch.h"
#endif
```

从工程根目录配置产物目录内独立的 CPU 构建目录，保留工程原有的其他选项：

```text
cmake -S . -B "<artifact-dir>/build-cpu" -DCMAKE_ASC_RUN_MODE=cpu -DCMAKE_ASC_ARCHITECTURES=<npu-arch>
cmake --build "<artifact-dir>/build-cpu" --parallel
<cpu-executable> <工程运行参数>
```

`<npu-arch>` 表示目标架构，例如 `dav-2201`、`dav-3510`；`<cpu-executable>` 使用实际生成路径。官方 Add 样例列出的 A2/A3 条件为 CANN ≥ 9.0.0，950PR/950DT 条件为 CANN ≥ 9.1.0。这是该样例的支持范围，不代表所有算子或接口的支持承诺。[样例说明][cpu-source-20]

### 已有 ICPU_RUN_KF 工程

9.0.0 文档示例通过 `tikicpulib.h`、`GmAlloc/GmFree` 和 `ICPU_RUN_KF` 组织 CPU 调试程序，并用工程的 `SOC_VERSION` 选择目标产品。已有这类工程时，沿用其内存分配、KernelMode、调用和数据校验方式；只替换当前需要调试的 Kernel 或输入。[旧工程示例][cpu-source-1]

```text
cmake -S . -B "<artifact-dir>/build-cpu" -DSOC_VERSION=<工程接受的产品名>
cmake --build "<artifact-dir>/build-cpu" --parallel
<cpu-executable> <工程运行参数>
```

两组命令分别适用于相应工程，不能仅添加一个变量就把任意 CMake 工程切换成 CPU 模式。文档推荐入口变化也不等于旧调用方式在新版本必然失效。

### 调试与结果解释

准备与失败场景一致的输入，先运行 CPU 程序及其 evaluator。以下示例用于不涉及核间同步的单子进程观察：

```text
gdb --args <cpu-executable> <工程运行参数>
(gdb) set follow-fork-mode child
(gdb) break <Kernel函数或源码位置>
(gdb) run
```

官方说明指出 CPU Debug 通过子进程模拟核函数执行，gdb 需要跟踪子进程才能进入对应断点。[gdb 说明][cpu-source-2]

该示例只停留在最先命中断点的子进程，其他进程继续运行，不适用于核间同步调试。此类问题先保留无断点运行；确需断点时按[官方 CPU 孪生调试指南的“调试多个子进程”](https://gitcode.com/cann/asc-devkit/blob/cdb4d800e937499d8fae2073099ccf61d1a44fc0/docs/zh/guide/programming_guide/debug_and_tuning/functional_debug/cpu_twin_debug.md#section13838280458)跟踪并切换相关进程，记录断点改变调度的影响，不能用当前等待或同步现象直接代表原始设备执行。

保存构建、运行状态和实际观察值。CPU 通过只说明该 CPU 路径在当前输入下通过；它不能单独证明 NPU 的舍入、流水或并发行为正确。对照方法见[CPU/NPU 差分](../../references/diagnosis/cpu-npu-differential.md)。切回 NPU 时使用独立构建目录或工程已有的干净重建方式，避免混用 CPU 产物。

[cpu-source-1]: https://gitcode.com/cann/asc-tools/blob/46a5b4d9ad924e5432e155cb62eea7585b448e62/docs/01_cpu_debug.md
[cpu-source-2]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/docs/01_cpu_debug.md
[cpu-source-13]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/examples/02_cpudebug/CMakeLists.txt
[cpu-source-20]: https://gitcode.com/cann/asc-tools/blob/950ab9eecf759dab94e546d0c8cc8c766d5d7716/examples/02_cpudebug/README.md
