# show_kernel_debug_data样例

## 概述

本样例基于Add算子，演示kernel侧算子调试信息的获取并通过[show_kernel_debug_data工具](../../docs/04_show_kernel_debug_data.md)解析。算子相关描述请参考：[Add算子直调样例](https://gitcode.com/cann/asc-devkit/tree/master/examples/00_introduction/01_add/basic_api_tque_add).

## 支持的产品

- Ascend 950PR/Ascend 950DT
- Atlas A3 训练系列产品/Atlas A3 推理系列产品
- Atlas A2 训练系列产品/Atlas A2 推理系列产品

## 目录结构介绍

```
├── 01_add
│   ├── acl.json                // Dump配置文件
│   ├── add.asc                 // Ascend C算子实现
│   └── CMakeLists.txt          // 编译工程文件
```

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

- 执行如下命令，若能正常显示--help或-h信息，则表示工具环境正常，功能可正常使用。
  ```bash
  show_kernel_debug_data -h
  ```

- 样例执行
  ```bash
  mkdir -p build output && cd build;   # 创建并进入build目录
  cmake ..;make -j;             # 编译工程
  # 在build目录执行以下内容
  ./add                         # 执行样例
  ```
  执行结果如下，说明精度对比成功。
  ```bash
  [Success] Case accuracy is verification passed.
  ```
  执行完成后将在当前目录生kernel调试信息bin文件，落在配置的路径下，例如:

    ```
    ${git_clone_path}/examples/01_show_kernel_debug_data/output
    └── 202xxxxxxxxxxx          // 待解析的dump二进制文件，包含kernel侧打印信息
        ├── asc_kernel_data_xxx.bin
        ├── ...
        └── asc_kernel_data_xxx.bin

- 调用show_kernel_debug_data工具解析

  使用命令行方式调用show_kernel_debug_data工具解析调试信息。

    ```bash
    mkdir dump_info_output
    show_kernel_debug_data ../output dump_info_output
    ```

    运行后终端显示如下打印信息(AscendC::print打印)，如需保存解析日志，需设置环境变量`ASCEND_GLOBAL_LOG_LEVEL=1`：

    ```
    log file saves to  ./dump_info_output/PARSER_20251022074515310995/parser.log
    write dump workspace result: ./dump_info_output/PARSER_20251022074515310995/dump_data
    ================ block.0 begin ==============
    fmt string int: 291
    fmt string int: 291
    fmt string float: 3.140000
    fmt string float: 3.140000
    ================ block.0 end ================
    ================ block.1 begin ==============
    fmt string int: 291
    fmt string int: 291
    fmt string float: 3.140000
    fmt string float: 3.140000
    ================ block.1 end ================
    ...
    ================ block.7 begin ==============
    fmt string int: 291
    fmt string int: 291
    fmt string float: 3.140000
    fmt string float: 3.140000
    ================ block.7 end ================
    ```

    结果目录结构如下:

    ```
    ${git_clone_path}/examples/01_show_kernel_debug_data/build/dump_info_output
    └── PARSER_20251022074515310995
        ├── dump_data
        │   ├── 0
        │   │   ├── core_0_index_0_loop_0.bin
        │   │   ├── core_0_index_0_loop_0.txt     // core0 desc0 progress0打印信息
        ...
        │   │   ├── core_0_index_2_loop_15.bin
        │   │   ├── core_0_index_2_loop_15.txt
        │   │   └── time_stamp_core_0.csv         // 时间戳信息
        │   ├── 1
        │   ├── 2
        │   ├── 3
        │   ├── 4
        │   ├── 5
        │   ├── 6
        │   ├── 7
        │   └── index_dtype.json                  // index与数据类型的映射关系
        └── parser.log                            // 工具解析日志
    ```

    其中dump_data目录下的0,1,2,...,7为8个核各自的打印信息。\
    index0、index1、index2分别对应代码中Dumptensor第二个参数desc=0、desc=1、desc=2所在函数调用代码的打印，对应到本例分别为xLocal, yLocal, zLocal的打印：

    ```
    AscendC::DumpTensor(xLocal[64], 0, 16);
    AscendC::DumpTensor(yLocal[64], 1, 16);
    AscendC::DumpTensor(zLocal[64], 2, 16);
    ```

    core_0_index_0_loop_x.*中x的取值是0-15，对应Block1，xLocal切分的每个tileLength大小的数据打印。
