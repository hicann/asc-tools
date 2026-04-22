# msobjdump样例

## 概述

本样例基于MatmulLeakyRelu算子，演示融合编译场景下`msobjdump`工具的使用方式。样例通过编译[matmul_leakyrelu.asc](./matmul_leakyrelu.asc)生成融合编译产物，再对生成的ELF文件执行解析。`msobjdump`工具的详细说明请参考[msobjdump工具](../../docs/03_msobjdump.md)。

## 支持的产品

- Ascend 950PR/Ascend 950DT
- Atlas A3 训练系列产品/Atlas A3 推理系列产品
- Atlas A2 训练系列产品/Atlas A2 推理系列产品

## 目录结构介绍

```
├── 04_msobjdump
│   ├── CMakeLists.txt          // 编译工程文件
│   ├── data_utils.h            // 数据读入写出函数
│   ├── matmul_leakyrelu.asc    // Ascend C算子实现 & 调用样例
│   └── scripts
│       ├── gen_data.py         // 输入数据和真值数据生成脚本文件
│       └── verify_result.py    // 真值对比文件
```

## 算子描述

- 样例功能：  
  MatmulLeakyRelu的计算公式为：
  ```
  C = A * B + Bias
  C = C > 0 ? C : C * 0.001
  ```
  样例参数M = 1024，K = 256，N = 640，样例规格如下表所示：
  <table>
  <tr><td rowspan="1" align="center">样例类型(OpType)</td><td colspan="4" align="center">MatmulLeakyRelu</td></tr>
  <tr><td rowspan="4" align="center">样例输入</td><td align="center">name</td><td align="center">shape</td><td align="center">data type</td><td align="center">format</td></tr>
  <tr><td align="center">A</td><td align="center">[M, K]</td><td align="center">float16</td><td align="center">ND</td></tr>
  <tr><td align="center">B</td><td align="center">[K, N]</td><td align="center">float16</td><td align="center">ND</td></tr>
  <tr><td align="center">Bias</td><td align="center">[N]</td><td align="center">float</td><td align="center">ND</td></tr>
  <tr><td rowspan="1" align="center">样例输出</td><td align="center">C</td><td align="center">[M, N]</td><td align="center">float</td><td align="center">ND</td></tr>
  <tr><td rowspan="1" align="center">核函数名</td><td colspan="4" align="center">matmul_leakyrelu_custom</td></tr>
  </table>

- 样例实现：
  - 实现流程
    - 通过GenerateTiling实现host侧的Tiling计算
    - 通过CalcGMOffset完成分核计算
    - 通过Iterate接口完成矩阵乘计算
    - 通过LeakyRelu实现激活函数计算

  - 调用实现  
    使用内核调用符<<<>>>调用核函数。

## 编译运行

在本样例根目录下执行如下步骤，编译并执行算子。
- 配置环境变量  
  请根据当前环境上CANN开发套件包的[安装方式](../../docs/00_quick_start.md#prepare&install)，选择对应配置环境变量的命令。
  - 默认路径，root用户安装CANN软件包
    ```bash
    source /usr/local/Ascend/cann/set_env.sh
    ```

  - 默认路径，非root用户安装CANN软件包
    ```bash
    source $HOME/Ascend/cann/set_env.sh
    ```

  - 指定路径install_path，安装CANN软件包
    ```bash
    source ${install_path}/cann/set_env.sh
    ```

- 执行如下命令，若能正常显示-h信息，则表示工具环境正常，功能可正常使用。
  ```bash
  msobjdump -h
  ```

- 样例执行
  ```bash
  mkdir -p build && cd build;   # 创建并进入build目录
  cmake ..;make -j;             # 编译工程
  python3 ../scripts/gen_data.py   # 生成测试输入数据
  ./demo                        # 执行编译生成的可执行程序，执行样例
  python3 ../scripts/verify_result.py output/output.bin output/golden.bin   # 验证输出结果是否正确，确认算法逻辑正确
  ```
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
    RUNTIME_IMPLICIT_INFO: DOUBLE_PAGE_TABLE_ADDR
    RUNTIME_IMPLICIT_INFO: FFTS_ADDR
    VERSION: 1
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
    CROSS_CORE_SYNC: USE_SYN
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
