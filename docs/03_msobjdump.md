<!--声明：本文使用[Creative Commons License version 4.0](https://creativecommons.org/licenses/by/4.0/legalcode)许可协议，转载、引用或修改等操作请遵循此许可协议。-->
# msobjdump  


## 概述
本工具主要针对Kernel直调算子开发与工程化算子开发编译生成的算子ELF文件（Executable and Linkable Format）提供解析和解压功能，并将结果信息以可读形式呈现，方便开发者直观获得kernel文件信息。关于本工具的详细介绍请参考《[Ascend C算子开发](https://hiascend.com/document/redirect/CannCommunityOpdevAscendC)》中的“msobjdump工具”。


## 工具安装

本工具跟随CANN软件包发布，请参考[环境搭建](00_quick_start.md)进行使用工具前必要的环境准备。  
-  执行如下命令设置环境变量。  
    ```
    source ${install_path}/latest/toolkit/bin/setenv.bash
    ```  
-  执行如下命令，若能正常显示--help或-h信息，则表示工具环境正常，功能可正常使用。
    ```
    msobjdump -h
    ```

## 命令格式

-  解析ELF文件的命令
    ```
    msobjdump --dump-elf <elf_file> [--verbose]
    ```  
    --dump-elf <elf_file>为必选，表示待解析ELF文件路径。[--verbose]为可选，用于开启ELF文件中全量打印device信息功能。

-  解压ELF文件的命令
    ```
    msobjdump --extract-elf <elf_file> [--out-dir <out_path>]
    ```  
    --extract-elf <elf_file>为必选，表示待解析ELF文件路径。[--out-dir <out_path>]为可选，用于设置解压文件的落盘路径。

-  获取ELF文件列表的命令
    ```
    msobjdump --list-elf <elf_file>
    ```
    --list-elf <elf_file>为可选，获取ELF文件中包含的device信息文件列表，并打印显示。

## 使用样例（Kernel直调算子工程）
以[matmul_kernellaunch](../examples/02_matmul_kernellaunch/README.md)算子为例（NPU模式），假设\${cmake_install_dir}为算子Cmake编译产物根目录，目录结构如下。

```
out
├── lib
│   ├── libascendc_kernels_npu.so
├── include
│   ├── ascendc_kernels_npu
│           ├── aclrtlaunch_matmul_custom.h
│           ├── aclrtlaunch_triple_chevrons_func.h
├── bin
│   ├── ascendc_kernels_bbit
```
 
工具对编译生成的库文件（如*.so、*.a等）进行解析和解压，功能实现命令样例如下：
-  解析包含device信息的库文件  
    支持两种打印方式。  
    -   简单打印
        ```
        msobjdump --dump-elf ${cmake_install_dir}/out/lib/libascendc_kernels_npu.so
        ```
        执行上述命令，终端打印基础device信息，示例如下：

        ```
        ===========================
        [VERSION]: 1
        [TYPE COUNT]: 1
        ===========================
        [ELF FILE 0]: ascendxxxb1_ascendc_kernels_npu_0_mix.o
        [KERNEL TYPE]: mix
        [KERNEL LEN]: 511560
        [ASCEND META]: None
        ```
    -   全量打印
        ```
        msobjdump --dump-elf ${cmake_install_dir}/out/lib/libascendc_kernels_npu.so --verbose
        ```  
        执行上述命令，终端打印所有device信息，示例如下：

        ```
        ===========================
        [VERSION]: 1
        [TYPE COUNT]: 1
        ===========================
        [ELF FILE 0]: ascendxxxb1_ascendc_kernels_npu_0_mix.o
        [KERNEL TYPE]: mix
        [KERNEL LEN]: 511560
        [ASCEND META]: None
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
          Start of section headers:          510280 (bytes into file)
          Flags:                             0x940000
          Size of this header:               64 (bytes)
          Size of program headers:           56 (bytes)
          Number of program headers:         2
          Size of section headers:           64 (bytes)
          Number of section headers:         20
          Section header string table index: 18

        Section Headers:
          [Nr] Name              Type            Address          Off    Size   ES Flg Lk Inf Al
          [ 0]                   NULL            0000000000000000 000000 000000 00      0   0  0
          [ 1] .text             PROGBITS        0000000000000000 0000b0 010a08 00  AX  0   0  4
          .....................................................................................
          [19] .strtab           STRTAB          0000000000000000 071278 00b6cb 00      0   0  1
        Key to Flags:
          W (write), A (alloc), X (execute), M (merge), S (strings), I (info),
          L (link order), O (extra OS processing required), G (group), T (TLS),
          C (compressed), x (unknown), o (OS specific), E (exclude),
          D (mbind), p (processor specific)

        There are no section groups in this file.

        Program Headers:
          Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
          LOAD           0x0000b0 0x0000000000000000 0x0000000000000000 0x010aa8 0x010aa8 R E 0x1000
          GNU_STACK      0x000000 0x0000000000000000 0x0000000000000000 0x000000 0x000000 RW  0
        ......
        ```
-   解压包含device信息的库文件并落盘
    ```
    msobjdump --extract-elf ${cmake_install_dir}/out/lib/libascendc_kernels_npu.so
    ```
    执行上述命令，默认在当前执行路径下落盘ascendxxxb1_ascendc_kernels_npu_0_mix.o文件。
-   获取包含device信息的库文件列表
    ```
    msobjdump --list-elf ${cmake_install_dir}/out/lib/libascendc_kernels_npu.so
    ```
    执行上述命令，终端会打印所有文件，屏显信息形如：

    ```
    ELF file    0: ascendxxxb1_ascendc_kernels_npu_0_mix.o
    ```
