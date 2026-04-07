<!--声明：本文使用[Creative Commons License version 4.0](https://creativecommons.org/licenses/by/4.0/legalcode)许可协议，转载、引用或修改等操作请遵循此许可协议。-->
# msopgen使用新自定义算子工程模板样例  

## 概述

本样例介绍msopgen工具按照新算子工程模板生成新自定义算子工程。以AddCustom算子为例，演示如何使用msopgen工具生成算子工程模板、构建算子包、安装算子以及验证算子精度。

### msopgen工具简介

[msopgen](https://gitcode.com/Ascend/msopgen)是昇腾AI自定义算子开发工具，用于生成自定义算子工程模板，帮助开发者快速搭建算子开发环境。通过提供算子原型定义文件，msopgen可以自动生成完整的算子工程框架，包括CMake构建系统、框架适配层等。

## 支持的产品

- Ascend 950PR/Ascend 950DT
- Atlas A3 训练系列产品/Atlas A3 推理系列产品
- Atlas A2 训练系列产品/Atlas A2 推理系列产品

## 算子介绍

AddCustom算子实现两个张量的逐元素相加操作：`z = x + y`

- 输入：x（fp16）、y（fp16）
- 输出：z（fp16）
- 数据格式：ND（N-dimensional）
- 支持的数据类型：float16

## 目录结构介绍

### msopgen工具生成前目录结构
```
├── aclnn_invocation                // ACL NN API调用示例，用于验证自定义算子
└── op_dev                          // 算子开发目录，存放算子源码
    ├── add_custom.json             // 算子原型定义文件（输入输出描述）
    ├── op_host                     // Host端代码目录
    │   └── add_custom.cpp          // Tiling实现和算子定义注册
    └── op_kernel                   // Kernel端代码目录
        ├── add_custom.cpp          // 核函数实现
        └── add_custom_tiling.h     // Tiling数据结构定义
```

### msopgen工具生成后目录结构
```
├── aclnn_invocation                // ACL NN API调用示例（保持不变）
├── op_dev                          // 算子开发目录，存放算子源码
└── custom_op                       // msopgen生成的自定义算子工程目录
    ├── build.sh                    // 构建脚本
    ├── CMakeLists.txt              // CMake配置
    ├── CMakePresets.json           // CMake预设配置
    ├── framework                   // 框架适配层
    │   ├── CMakeLists.txt
    │   └── tf_plugin               // TensorFlow插件
    │       ├── CMakeLists.txt
    │       └── tensorflow_add_custom_plugin.cc
    ├── op_host                     // Host端代码
    │   ├── add_custom.cpp          // 拷贝host侧算子实现文件，用户可自行修改替换
    │   └── CMakeLists.txt
    └── op_kernel                   // Kernel端代码
        ├── add_custom.cpp          // 拷贝kernel侧算子实现文件，用户可自行修改替换
        ├── add_custom_tiling.h     // 拷贝Tiling数据结构定义，用户可自行修改替换
        └── CMakeLists.txt
```

## aclnn_invocation说明

本样例中的 `aclnn_invocation` 目录提供了使用ACLNN API调用自定义算子的完整示例。该示例与 [asc-devkit仓库](https://gitcode.com/cann/asc-devkit/tree/master/examples/01_simd_cpp_api/02_features/01_invocation/aclnn_invocation) 中的示例完全一致，用于演示如何通过ACLNN API加载和执行自定义算子。

更多ACLNN API调用示例和详细说明，请参考：[ACLNN API调用示例](https://gitcode.com/cann/asc-devkit/tree/master/examples/01_simd_cpp_api/02_features/01_invocation/aclnn_invocation)

## 编译运行

### 环境准备

在本样例根目录下执行如下步骤，编译并执行样例。

- 配置环境变量
  
  请根据当前环境上CANN开发套件包的[安装方式](../../../../../docs/quick_start.md#prepare&install)，选择对应配置环境变量的命令。
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

### 编译执行步骤

在本样例根目录下执行如下步骤，运行该样例。
1. **生成自定义算子工程**
   ```bash
   msopgen gen -i ./op_dev/add_custom.json -f <framework> -c ai_core-<soc_version> -lan cpp -out ./custom_op
   ```
   - soc_version：昇腾AI处理器型号，如果无法确定具体的soc_version，则在安装昇腾AI处理器的服务器执行`npu-smi info`命令进行查询，在查询到的"Name"前增加Ascend信息，例如"Name"对应取值为xxxyy，实际配置的soc_version值为Ascendxxxyy。
   - framework： 框架类型。默认为TensorFlow框架，默认值：tf或tensorflow。 其他可选值可参考[msopgen工具用户指南](https://gitcode.com/Ascend/msopgen/blob/master/docs/zh/user_guide/msopgen_user_guide.md)
   - 若用户多次运行msopgen命令生成算子工程，请先删除已生成的custom_op目录。

2. **复制源码到生成目录**
   ```bash
   cp -rf ./op_dev/op_kernel custom_op
   cp -rf ./op_dev/op_host custom_op
   ```

3. **构建自定义算子包**
   ```bash
   cd custom_op
   bash build.sh
   ```

4. **安装自定义算子包**
   ```bash
   cd build_out
   ./custom_opp_<target_os>_<target_architecture>.run
   ```

5. **编译并运行验证程序**
   ```bash
   cd ../../aclnn_invocation
   mkdir -p build
   cd build
   cmake .. && make -j
   ./execute_add_op
   ```
6. **执行结果如下，说明执行成功**
   ```log
   test pass
   ```
