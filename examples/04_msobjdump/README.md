# msobjdump样例

## 概述

本样例基于MatmulLeakyRelu算子演示融合编译场景下`msobjdump`工具的使用方式。样例通过编译[matmul_leakyrelu.asc](./matmul_leakyrelu.asc)生成融合编译产物，再使用`msobjdump`解析生成的ELF文件。`msobjdump`工具的详细说明请参考[msobjdump工具](../../docs/03_msobjdump.md)。

## 本样例支持的产品及CANN软件版本

| 产品 | CANN软件版本 |
|------|-------------|
| Ascend 950PR/Ascend 950DT | >= CANN 9.1.0 |
| Atlas A3 训练系列产品/Atlas A3 推理系列产品 | >= CANN 9.0.0 |
| Atlas A2 训练系列产品/Atlas A2 推理系列产品 | >= CANN 9.0.0 |

## 目录结构介绍

```
├── 04_msobjdump
│   ├── CMakeLists.txt          // 编译工程文件
│   ├── data_utils.h            // 数据读入写出函数
│   ├── matmul_leakyrelu.asc    // Ascend C算子实现 & 调用样例
│   ├── scripts
│   │   ├── gen_data.py         // 输入数据和真值数据生成脚本
│   │   └── verify_result.py    // 真值对比脚本
│   └── README.md               // 样例说明文档
```

## 样例描述

- 样例功能：

  MatmulLeakyRelu计算公式为：

  ```
  C = A * B + Bias
  C = C > 0 ? C : C * 0.001
  ```

- 样例规格：
  <table border="2" align="center">
  <caption>表1：MatmulLeakyRelu样例规格描述</caption>
  <tr><td rowspan="1" align="center">样例类型(OpType)</td><td colspan="4" align="center">MatmulLeakyRelu</td></tr>
  <tr><td rowspan="4" align="center">样例输入</td><td align="center">name</td><td align="center">shape</td><td align="center">data type</td><td align="center">format</td></tr>
  <tr><td align="center">A</td><td align="center">[1024, 256]</td><td align="center">float16</td><td align="center">ND</td></tr>
  <tr><td align="center">B</td><td align="center">[256, 640]</td><td align="center">float16</td><td align="center">ND</td></tr>
  <tr><td align="center">Bias</td><td align="center">[640]</td><td align="center">float</td><td align="center">ND</td></tr>
  <tr><td rowspan="1" align="center">样例输出</td><td align="center">C</td><td align="center">[1024, 640]</td><td align="center">float</td><td align="center">ND</td></tr>
  <tr><td rowspan="1" align="center">核函数名</td><td colspan="4" align="center">matmul_leakyrelu_custom</td></tr>
  </table>

- 样例实现：

  Host侧通过`GenerateTiling`生成Tiling参数；Kernel侧通过`CalcGMOffset`完成分核地址计算，通过`matmulObj.Iterate`完成矩阵乘计算，再通过`LeakyRelu`完成激活函数计算，最后将结果搬回Global Memory。

## 编译运行

在本样例根目录下执行如下步骤，编译并执行样例。

- 配置环境变量

  请根据当前环境上CANN开发套件包的[安装方式](../../docs/00_quick_start.md#prepare&install)，配置环境变量。

  ```bash
  source ${install_path}/cann/set_env.sh
  ```

  > **说明：** `${install_path}` 为CANN包安装目录，未指定安装目录时默认安装至 `/usr/local/Ascend` 下。

- 检查工具环境

  执行如下命令，若能正常显示帮助信息，则表示工具环境正常。

  ```bash
  msobjdump -h
  ```

- 样例执行

  在本样例目录下执行如下命令。

  ```bash
  mkdir -p build && cd build;
  cmake -DCMAKE_ASC_ARCHITECTURES=dav-2201 ..;make -j;
  python3 ../scripts/gen_data.py
  ./demo
  python3 ../scripts/verify_result.py output/output.bin output/golden.bin
  ```

- 编译选项说明

  | 选项 | 可选值 | 说明 |
  |------|--------|------|
  | `CMAKE_ASC_ARCHITECTURES` | `dav-2201`（默认）、`dav-3510` | NPU 架构：<br>&bull; dav-2201，对应 Atlas A2 训练系列产品/Atlas A2 推理系列产品和 Atlas A3 训练系列产品/Atlas A3 推理系列产品<br>&bull; dav-3510，对应 Ascend 950PR/Ascend 950DT |

- 执行结果

  执行结果如下，说明精度对比成功。

  ```bash
  test pass!
  ```

- 调用msobjdump工具解析

  样例中的`demo`为融合编译生成的ELF文件。若该ELF中包含`.aicore_binary`段，`msobjdump`会自动提取该段内容并继续解析，无需手工拆分中间文件。

  - 解析融合编译产物
    ```bash
    msobjdump --dump-elf ./demo
    ```

    本样例实际输出如下：

    ```Plain Text
    .ascend.meta META INFO
    RUNTIME_IMPLICIT_INFO: L2Cache Hint Flag
    RUNTIME_IMPLICIT_INFO: Hardware Sync Flag
    VERSION: 1
    RUNTIME_IMPLICIT_INFO: SIMD Printf Flag
    .ascend.meta. [0]: _Z23matmul_leakyrelu_customPhS_S_S_S_N7AscendC6tiling11TCubeTilingE_mix_aic
    KERNEL_TYPE: MIX_AIC_MAIN
    CROSS_CORE_SYNC: USE_SYNC
    MIX_TASK_RATION: [1:2]
    .ascend.meta. [0]: _Z23matmul_leakyrelu_customPhS_S_S_S_N7AscendC6tiling11TCubeTilingE_mix_aiv
    KERNEL_TYPE: MIX_AIC_MAIN
    CROSS_CORE_SYNC: USE_SYNC
    MIX_TASK_RATION: [1:2]
    ```

  - 全量打印融合编译产物中的device信息
    ```bash
    msobjdump --dump-elf ./demo --verbose
    ```
    本样例实际输出如下：
    ```Plain Text
    .ascend.meta META INFO
    RUNTIME_IMPLICIT_INFO: DOUBLE_PAGE_TABLE_ADDR
    RUNTIME_IMPLICIT_INFO: FFTS_ADDR
    VERSION: 1
    RUNTIME_IMPLICIT_INFO: SIMD_TRACE_SPACE
    .ascend.meta. [0]: _Z23matmul_leakyrelu_customPhS_S_S_S_N7AscendC6tiling11TCubeTilingE_mix_aic
    KERNEL_TYPE: MIX_AIC_MAIN
    CROSS_CORE_SYNC: USE_SYNC
    MIX_TASK_RATION: [1:2]
    .ascend.meta. [0]: _Z23matmul_leakyrelu_customPhS_S_S_S_N7AscendC6tiling11TCubeTilingE_mix_aiv
    KERNEL_TYPE: MIX_AIC_MAIN
    CROSS_CORE_SYNC: USE_SYNC
    MIX_TASK_RATION: [1:2]
    ====== [elf heard infos] ======
    ELF Header:
      Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
      Class:                             ELF64
      Data:                              2's complement, little endian
      Version:                           1 (current)
      OS/ABI:                            UNIX - System V
      ABI Version:                       0
      Type:                              EXEC (Executable file)
      Machine:                           <unknown>: 0x1029
      Version:                           0x1
      Entry point address:               0x0
      Start of program headers:          64 (bytes into file)
      Start of section headers:          33504 (bytes into file)
      Flags:                             0x940000
      Size of this header:               64 (bytes)
      Size of program headers:           56 (bytes)
      Number of program headers:         3
      Size of section headers:           64 (bytes)
      Number of section headers:         16
      Section header string table index: 14

    Section Headers:
      [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
      [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
      [ 1] .text             PROGBITS        0000000000000000 0000e8 006c94 00  AX  0   0  4
      ......................................................................................
      [15] .strtab           STRTAB          0000000000000000 007ce0 0005fc 00      0   0  1
    Key to Flags:
      W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
      L (link order), O (extra OS processing required), G (group), T (TLS),
      C (compressed), x (unknown), o (OS specific), E (exclude),
      D (mbind), p (processor specific)

    There are no section groups in this file.

    Program Headers:
      Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
      LOAD           0x0000e8 0x0000000000000000 0x0000000000000000 0x006ca7 0x006ca7 R E 0x1000
      LOAD           0x0070e8 0x0000000000007000 0x0000000000007000 0x000210 0x000210 RW  0x1000
      GNU_STACK      0x000000 0x0000000000000000 0x0000000000000000 0x000000 0x000000 RW  0

    ......
    ```

  - 获取融合编译产物中的ELF文件列表
    ```bash
    msobjdump --list-elf ./demo
    ```

    对当前样例产物，终端提示如下：

    ```Plain Text
    ELF file    0: demo.aicore.o
    ```

  - 解压融合编译产物中的ELF文件
    ```bash
    mkdir -p objdump_out
    msobjdump --extract-elf ./demo
    ```

    执行上述命令，默认在当前执行路径下落盘`demo.aicore.o`文件，若需指定路径可通过--out-dir进行设置。
