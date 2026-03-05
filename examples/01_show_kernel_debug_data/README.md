# show_kernel_debug_data样例

## 概述

kernel侧算子调试信息（AscendC::DumpTensor, AscendC::printf等）可通过Dump配置后进行获取。show_kernel_debug_data工具提供了离线解析能力，帮助用户获取并解析调试信息（将bin文件解析成可读格式）。

本样例基于Add算子，演示show_kernel_debug_data工具获取并解析调试信息。算子相关描述请参考：[Add算子直调样例](https://gitcode.com/cann/asc-devkit/tree/master/examples/00_introduction/01_add/basic_api_tque_add).

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

## 使用方法

- **命令行方式**

    ```
    show_kernel_debug_data  `<bin_file_path>`  [`<output_path>`]
    ```

    | 参数 | 可选/必选 | 说明 |
    |-------|-------|-------|
    |`bin_file_path`| 必选 | kernel侧调试信息落盘路径，支持bin文件或目录。目录模式下会递归收集`.bin`文件并统一解析。 |
    |`output_path`| 可选 | 解析结果保存路径，例如`"/output_dir"`。默认是当前命令行执行目录。若目录不存在，工具会自动创建。 |

- **API方式**

    show\_kernel\_debug\_data接口说明：

    <table>
        <tr>
            <td ><strong>函数原型</strong></td>
            <td colspan="2">def show_kernel_debug_data(bin_file_path: str, output_path: str = './') -> None</td>
        </tr >
        <tr>
            <td ><strong>函数功能</strong></td>
            <td colspan="2">获取kernel侧调试信息并解析成可读文件。</td>
        </tr >
        <tr >
            <td rowspan="2"><strong>参数（IN）</strong></td>
            <td>bin_file_path</td>
            <td>kernel侧调试信息落盘路径，支持bin文件或目录，字符串类型。</td>
        </tr>
        <tr>
            <td>output_path</td>
            <td>解析结果保存路径，字符串类型。默认是当前接口调用脚本目录，目录不存在时自动创建。</td>
        </tr>
        <tr>
            <td><strong>参数（OUT）</strong></td>
            <td>NA</td>
                <td>-</td>
        </tr>
        <tr>
                <td><strong>返回值</strong></td>
            <td>NA</td>
                <td>-</td>
        </tr>
        <tr>
            <td ><strong>使用约束</strong></td>
            <td colspan="2">无</td>
        </tr >
        <tr>
            <td ><strong>调用示例</strong></td>
            <td colspan="2">from show_kernel_debug_data import show_kernel_debug_data<br>
            show_kernel_debug_data("./input/dump_workspace.bin")</td>
        </tr >
    </table>

- Dump配置

  通过[aclInit接口](https://www.hiascend.com/document/detail/zh/canncommercial/850/API/appdevgapi/aclcppdevg_03_0022.html)启用Dump配置，需配置dump_path参数设置保存Dump数据的路径，配置dump_kernel_data参数开启Dump功能，配置文件内容的示例如下：
    ```json
    {
        "dump":{
            "dump_kernel_data":"all",
            "dump_path":"../output"
        }
    }
    ```
    - dump_kernel_data：指定导出数据的类型，支持配置多个类型，用英文逗号隔开。当前支持如下类型：
        - all：导出以下所有类型调测的输出数据。
        - printf: 导出AscendC::printf/PRINTF调测的输出数据。
        - tensor: 导出AscendC::DumpTensor调测的输出数据。
        - assert: 导出ascend_assert调测的输出数据。
        - timestamp: 导出AscendC::PrintTimestamp获取的时间戳信息。
    - dump_path：启用算子Kernel调测信息Dump功能时，dump_path必须配置，支持配置绝对路径或相对路径。

    除上述方式之外，还可以通过环境变量ASCEND_DUMP_PATH和ASCEND_WORK_PATH配置Dump存储路径。Dump文件存路径的优先级如下：ASCEND_DUMP_PATH > ASCEND_WORK_PATH > 配置文件中的dump_path。

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
    ${git_clone_path}/examples/01_show_kernel_debug_data/dump_info_output
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
