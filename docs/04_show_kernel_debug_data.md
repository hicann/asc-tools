# show_kernel_debug_data

## 概述

静态图场景下，整图下沉到NPU侧执行，kernel侧算子调试信息（AscendC::DumpTensor, AscendC::print）需要在模型执行结束后才能获取。
show_kernel_debug_data工具提供了离线解析能力，帮助用户获取并解析调试信息（将bin文件解析成可读格式）。

关于该工具的详细说明，请参考[《Ascend C算子开发》](https://hiascend.com/document/redirect/CannCommunityOpdevAscendC)中的“show_kernel_debug_data工具”。

## 环境准备

本工具跟随CANN软件包发布，请参考[环境搭建](00_quick_start.md)进行使用工具前必要的环境准备。

对于本工具的演示样例，还需要额外安装opp_legacy包。根据实际环境，下载对应`Ascend-cann-${soc_name}-ops_8.5.0-beta.1_linux-${arch}.run`包，下载链接为:
[910B x86_64 ops包](https://ascend.devcloud.huaweicloud.com/cann/run/software/8.5.0-beta.1/x86_64/Ascend-cann-910b-ops_8.5.0-beta.1_linux-x86_64.run);
[910B aarch64 ops包](https://ascend.devcloud.huaweicloud.com/cann/run/software/8.5.0-beta.1/aarch64/Ascend-cann-910b-ops_8.5.0-beta.1_linux-aarch64.run);
[910C x86_64 ops包](https://ascend.devcloud.huaweicloud.com/cann/run/software/8.5.0-beta.1/x86_64/Ascend-cann-A3-ops_8.5.0-beta.1_linux-x86_64.run);
[910C aarch64 ops包](https://ascend.devcloud.huaweicloud.com/cann/run/software/8.5.0-beta.1/aarch64/Ascend-cann-A3-ops_8.5.0-beta.1_linux-aarch64.run)。

    ```bash
    # 确保安装包具有可执行权限
    chmod +x Ascend-cann-${soc_name}-ops_8.5.0-beta.1_linux-${arch}.run
    # 安装命令
    ./Ascend-cann-${soc_name}-ops_8.5.0-beta.1_linux-${arch}.run --full --install-path=${install_path}
    ```
    - \$\{soc\_version\}：表示AI处理器型号（910B对于910b，910C对应A3）。
    - \$\{cann\_version\}：表示CANN包版本号。
    - \$\{arch\}：表示CPU架构，如aarch64、x86_64。
    - \$\{install\_path\}：表示指定安装路径。
    - 缺省--install-path时， 则使用默认路径安装。
    若使用root用户安装，安装完成后相关软件存储在“/usr/local/Ascend/latest”路径下；若使用非root用户安装，安装完成后相关软件存储在“$HOME/Ascend/latest”路径下。

-  执行如下命令设置环境变量。  
    ```
    source ${install_path}/latest/toolkit/bin/setenv.bash
    ```  

- 然后执行如下命令，若能正常显示--help或-h信息，则表示工具环境正常，功能可正常使用。
    ```
    show_kernel_debug_data -h
    ```

## 使用方法

- **命令行方式**

    ```
    show_kernel_debug_data  `<bin_file_path>`  [`<output_path>`]
    ```

    | 参数 | 可选/必选 | 说明 |
    |-------|-------|-------|
    |bin_file_path| 必选 | kernel侧调试信息落盘的bin文件路径，例如“/input/dump_workspace.bin”。 |
    |output_path| 可选 | 解析结果的保存路径，例如“/output_dir”。默认是当前命令行执行目录下。 |

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
            <td>kernel侧调试信息落盘的bin文件路径，字符串类型。</td>
        </tr>
        <tr>
            <td>output_path</td>
            <td>解析结果的保存路径，字符串类型，默认是当前接口调用脚本所在目录下。</td>
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
            show_kernel_debug_data(./input/dump_workspace.bin)</td>
        </tr >
    </table>

- **运行结果**

    运行show_kernel_debug_data工具对kernel侧调试信息bin文件进行解析后，AscendC::printf/PRINTF打印信息直接在终端显示，AscendC::DumpTensor打印信息记录在结果文件中，结果目录结构如下：

    ```
    ${output_path}
    └── PARSER_${timestamp}         // ${timestamp}表示时间戳。
    ├── dump_data               // 各个kernel上的AscendC::DumpTensor打印信息。
    └── parser.log              // 工具解析的日志，包含kernel侧日常流程和printf打印信息。
    ```

## 使用示例

本示例基于[1_add_framework_launch](../examples/01_add_frameworklaunch/README.md)样例运行。
通过在该样例的核函数实现中增加打印，获取kernel侧调试dump文件，再使用show_kernel_debug_data工具解析该dump文件获取用户添加的调试信息。

**目录结构介绍**

```
├ ${git_clone_path}/examples/01_add_frameworklaunch
...
├── AddCustom
├── AddCustom.json
├── install.sh
├── PytorchInvocation			// PytorchInvocation样例
└── README.md				// AddCustom算子工程样例说明
```

- **步骤1**：在AddCustom目录下的kernel侧算子实现文件add_custom.cpp中添加打印

    ```cpp
    // {CUR_DIR}/AddCustom/op_kernel/add_custom.cpp
    inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z, uint32_t totalLength, uint32_t tileNum)
    {
        this->blockLength = totalLength / AscendC::GetBlockNum();
        this->tileNum = tileNum;
        this->tileLength = this->blockLength / tileNum / BUFFER_NUM;

        // Add AscendC::printf to show debug info
        AscendC::printf("this->blockLength is: %u, this->tileLength is : %u\n", this->blockLength, this->tileLength);

        xGm.SetGlobalBuffer((__gm__ DTYPE_X *)x + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
        yGm.SetGlobalBuffer((__gm__ DTYPE_Y *)y + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
        zGm.SetGlobalBuffer((__gm__ DTYPE_Z *)z + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
        pipe.InitBuffer(inQueueX, BUFFER_NUM, this->tileLength * sizeof(DTYPE_X));
        pipe.InitBuffer(inQueueY, BUFFER_NUM, this->tileLength * sizeof(DTYPE_Y));
        pipe.InitBuffer(outQueueZ, BUFFER_NUM, this->tileLength * sizeof(DTYPE_Z));
    }
    inline void Compute(int32_t progress)
    {
        AscendC::LocalTensor<DTYPE_X> xLocal = inQueueX.DeQue<DTYPE_X>();
        AscendC::LocalTensor<DTYPE_Y> yLocal = inQueueY.DeQue<DTYPE_Y>();
        AscendC::LocalTensor<DTYPE_Z> zLocal = outQueueZ.AllocTensor<DTYPE_Z>();
        AscendC::Add(zLocal, xLocal, yLocal, this->tileLength);
        outQueueZ.EnQue<DTYPE_Z>(zLocal);
        inQueueX.FreeTensor(xLocal);
        inQueueY.FreeTensor(yLocal);

        // DumpTensor show input/output info
        AscendC::DumpTensor(xLocal[64], 0, 16);
        AscendC::DumpTensor(yLocal[64], 1, 16);
        AscendC::DumpTensor(zLocal[64], 2, 16);
        if (progress == 0) {
            // print int
            AscendC::printf("fmt string int: %d\n", 0x123);
            AscendC::PRINTF("fmt string int: %d\n", 0x123);
            // print float
            float a = 3.14;
            AscendC::printf("fmt string float: %f\n", a);
            AscendC::PRINTF("fmt string float: %f\n", a);
        }
    }
    ```

    上述修改主要在add_custom.cpp的init和Compute函数中增加了AscendC::DumpTensor, AscendC::printf/PRINTF打印。代码修改完成之后按照[AddCustom算子工程](../examples/01_add_frameworklaunch/README.md)的介绍完成算子的编译和安装。

- **步骤2**：算子入图并运行获取kernel侧调试信息

    详细步骤请参考[01_add_frameworklaunch样例下PytorchInvocation](../examples/01_add_frameworklaunch/PytorchInvocation/README.md)样例中的说明。

    运行完成后将在当前目录生kernel调试信息bin文件，落在printf目录下:

    ```
    ${git_clone_path}/examples/01_add_frameworklaunch/PytorchInvocation/printf/0/graph_1/1/0
    └── AddCustom.AddCustom.XXX          // 待解析的dump二进制文件，包含kernel侧打印信息
    ```

- **步骤3**：解析dump文件为用户可读内容

    使用命令行方式调用show_kernel_debug_data工具解析调试信息。

    ```bash
    mkdir dump_info_output
    show_kernel_debug_data ./printf/0/graph_1/1/0/AddCustom.AddCustom.XXX dump_info_output
    ```

    运行后终端显示如下打印信息(AscendC::print打印)：

    ```
    log file saves to  /root/samples/operator/ascendc/0_introduction/01_add_frameworklaunch/PytorchInvocation/dump_info_output/PARSER_20251022074515310995/parser.log
    write dump workspace result: /root/samples/operator/ascendc/0_introduction/01_add_frameworklaunch/PytorchInvocation/dump_info_output/PARSER_20251022074515310995/dump_data
    ================ block.0 begin ==============
    [Meta Info] block num: 8, core type: VEC, isMix: False
    CANN Version: 8.5.RC1, TimeStamp: 20251011001525507
    this->blockLength is: 2048, this->tileLength is : 128
    fmt string int: 291
    fmt string int: 291
    fmt string float: 3.140000
    fmt string float: 3.140000
    ================ block.0 end ================
    ================ block.1 begin ==============
    [Meta Info] block num: 8, core type: VEC, isMix: False
    CANN Version: 8.5.RC1, TimeStamp: 20251011001525507
    this->blockLength is: 2048, this->tileLength is : 128
    fmt string int: 291
    fmt string int: 291
    fmt string float: 3.140000
    fmt string float: 3.140000
    ================ block.1 end ================
    ...
    ================ block.7 begin ==============
    [Meta Info] block num: 8, core type: VEC, isMix: False
    CANN Version: 8.5.RC1, TimeStamp: 20251011001525507
    this->blockLength is: 2048, this->tileLength is : 128
    fmt string int: 291
    fmt string int: 291
    fmt string float: 3.140000
    fmt string float: 3.140000
    ================ block.7 end ================
    ```

    结果目录结构如下:

    ```
    ${git_clone_path}/examples/01_add_frameworklaunch/PytorchInvocation/dump_info_output
    └── PARSER_20251022074515310995
        ├── dump_data
        │   ├── 0
        │   │   ├── index_0        // core 0上DumpTensor desc0 打印信息
        │   │   │   ├── core_0_index_0_loop_0.bin
        │   │   │   ├── core_0_index_0_loop_0.txt	// core0 desc0 progress0打印信息
    ...
        │   │   │   ├── core_0_index_0_loop_15.bin
        │   │   │   ├── core_0_index_0_loop_15.txt
        │   │   ├── index_1        // core 0上DumpTensor desc1 打印信息
        │   │   └── index_2        // core 0上DumpTensor desc2 打印信息
        │   ├── 1
        │   ├── 2
        │   ├── 3
        │   ├── 4
        │   ├── 5
        │   ├── 6
        │   ├── 7
        │   └── index_dtype.json
        └── parser.log
    ```

    其中dump_data目录下的0,1,2,...,7为8个核各自的打印信息。\
    index0、index1、index2分别对应代码中Dumptensor第二个参数desc=0、desc=1、desc=2所在函数调用代码的打印，对应到本例分别为xLocal, yLocal, zLocal的打印：

    ```
    AscendC::DumpTensor(xLocal[64], 0, 16);
    AscendC::DumpTensor(yLocal[64], 1, 16);
    AscendC::DumpTensor(zLocal[64], 2, 16);
    ```

    index_0目录下的core_0_index_0_loop_x.*中x的取值是0-15，对应Block1，xLocal切分的每个tileLength大小的数据打印。其中core_0_index_0_loop_0.txt打印的AscendC::DumpTensor内容参考如下形式：

    ```
    0.2685546875,0.84765625,0.515625,0.5615234375,0.115234375,0.8291015625,0.36328125,0.533203125,
    0.8525390625,0.755859375,0.705078125,0.921875,0.2734375,0.1064453125,0.21484375,0.4736328125,
    ```

