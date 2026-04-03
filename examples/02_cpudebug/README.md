# CPU Debug直调样例说明

## 概述

本样例通过Ascend C编程语言实现了Add算子的CPU Debug调测。

## 支持的产品

- Ascend 950PR/Ascend 950DT
- Atlas A2 训练系列产品/Atlas A2 推理系列产品
- Atlas A3 训练系列产品/Atlas A3 推理系列产品
 
## 目录结构介绍

```
├── 02_cpudebug
│   ├── CMakeLists.txt          // 编译工程文件
│   └── add.asc                 // Ascend C算子实现 & 调用样例
```

## 算子描述

- 算子功能：  
CPU Debug介绍
  CPU Debug功能支持对CPU执行过程中的运行状态进行调试，主要通过GDB工具实现。GDB调试支持设置断点、查看寄存器和内存状态、单步执行、查看调用栈等常用调试操作。

  - Add算子介绍  
  Add算子实现了两个数据相加，返回相加结果的功能。对应的数学表达式为：  
  ```
  z = x + y
  ```
- 算子规格：  
  Add算子：  
  <table>
  <tr><td rowspan="1" align="center">算子类型(OpType)</td><td colspan="4" align="center">Add</td></tr>
  </tr>
  <tr><td rowspan="3" align="center">算子输入</td><td align="center">name</td><td align="center">shape</td><td align="center">data type</td><td align="center">format</td></tr>
  <tr><td align="center">x</td><td align="center">8 * 2048</td><td align="center">float</td><td align="center">ND</td></tr>
  <tr><td align="center">y</td><td align="center">8 * 2048</td><td align="center">float</td><td align="center">ND</td></tr>
  </tr>
  </tr>
  <tr><td rowspan="1" align="center">算子输出</td><td align="center">z</td><td align="center">8 * 2048</td><td align="center">float</td><td align="center">ND</td></tr>
  </tr>
  <tr><td rowspan="1" align="center">核函数名</td><td colspan="4" align="center">add_custom</td></tr>
  </table>

- 算子实现：  

  Add算子： 

  本样例中实现的是固定shape为8*2048的Add算子。

  - Kernel实现  

    Add算子的数学表达式为：
    ```
    z = x + y
    ```
    计算逻辑是：Ascend C提供的矢量计算接口的操作元素都为LocalTensor，输入数据需要先搬运进片上存储，然后使用计算接口完成两个输入参数相加，得到最终结果，再搬出到外部存储上。

    Add算子的实现流程分为3个基本任务：CopyIn，Compute，CopyOut。CopyIn任务负责将Global Memory上的输入Tensor xGm和yGm搬运到Local Memory，分别存储在xLocal、yLocal，Compute任务负责对xLocal、yLocal执行加法操作，计算结果存储在zLocal中，CopyOut任务负责将输出数据从zLocal搬运至Global Memory上的输出Tensor zGm中。

## 编译运行  

在本样例根目录下执行如下步骤，编译并执行算子。
- 环境准备  
  请参考[快速入门](../docs/00_quick_start.md#环境准备)完成环境准备。

- 样例执行
  请根据实际测试的 NPU 硬件架构选择对应的 `CMAKE_ASC_ARCHITECTURES` 参数
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

  执行结果如下，说明精度对比成功。
  ```bash
  [Success] Case accuracy is verification passed.
  ```  
- 进入gdb模式调试
  在上述指令中"./add"前加入"gdb --args"，再次执行指令即可进入gdb模式。
  ```bash
  gdb --args ./build/add
  ```