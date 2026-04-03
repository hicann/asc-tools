# cpu debug

## 概述

在算子部署到NPU上之前，CPU Debug工具帮助用户在CPU上进行功能和精度的基本验证。开发者使用Ascend C编写算子Kernel侧源码，通过bisheng编译器编译生成CPU域的可执行程序，即可使用gdb等常规调试手段对算子进行调试。

## 环境准备

请参考[快速入门](00_quick_start.md#环境准备)完成环境准备。


## 使用方法

以[cpudebug](../examples/02_cpudebug/)样例为例，只需以下两步即可开始CPU调试。

### 步骤1：添加头文件引用

在通过`<<<>>>`调用核函数的源文件中，添加如下代码：

```c
#ifdef ASCENDC_CPU_DEBUG
#include "cpu_debug_launch.h"
#endif
```
bisheng编译器在CPU调试模式下会通过该头文件对`<<<>>>`形式的核函数调用进行转义，从而在CPU上执行核函数，该修改不会影响代码在NPU模式下的编译运行。

### 步骤2：编译并运行

以dav-2201架构的NPU（如Ascend910B1）为例：

```bash
cmake -B build -DCMAKE_ASC_RUN_MODE=cpu -DCMAKE_ASC_ARCHITECTURES=dav-2201;
cmake --build build;
./build/add
```

- 编译选项说明

    | 选项 | 说明 |
    |------|------|
    | `CMAKE_ASC_RUN_MODE` | 指定为`cpu`, 开启CPU域编译 |
    | `CMAKE_ASC_ARCHITECTURES` | 指定NPU架构版本号，CMake会根据该值配置对应的CPU调试依赖库。<br>`dav-2201` 对应 Atlas A2/A3 系列，`dav-3510` 对应 Ascend 950PR/Ascend 950DT |

## 调试方法

编译生成的CPU域可执行程序支持通过gdb进行调试。gdb支持设置断点、查看寄存器和内存状态、单步执行、查看调用栈等常用调试操作。

CPU Debug通过为每个核函数启动单独的子进程来模拟NPU的执行逻辑，因此使用gdb调试时，需要设置`follow-fork-mode`让gdb跟踪子进程，才能在核函数内部断点调试。

基本用法如下：

```bash
gdb ./build/add
```

进入gdb后，先设置跟踪子进程模式：

```text
(gdb) set follow-fork-mode child
```

然后按需进行调试，常用操作：

```text
# 在核函数入口处设置断点
(gdb) break Compute

# 运行程序
(gdb) run

# 单步执行
(gdb) next

# 打印变量值
(gdb) print xLocal.GetValue(0)

# 继续执行到下一个断点
(gdb) continue
```

> **说明**：`set follow-fork-mode child` 告诉gdb在fork创建子进程时切换到子进程进行调试。如果不设置该选项，gdb默认跟踪父进程，将无法进入核函数内部。

## 切回NPU模式

CPU调试完成后，清除build目录并重新配置cmake即可切回NPU模式运行。步骤1中添加的`#ifdef`代码无需移除，在NPU模式下不会产生任何影响。

```bash
rm -r build;
cmake -B build -DCMAKE_ASC_ARCHITECTURES=dav-2201;
cmake --build build;
./build/add
```
