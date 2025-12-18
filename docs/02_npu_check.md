# npu_check

## 概述

Ascend C Tools提供的孪生调试分为debug功能和npu check功能，debug功能包含诸如是否合法使用接口，参数校验等，在此之上npu check提供了内存检查、内存生命周期管理、内存地址依赖管理、同步事件管理等功能。需要注意的是，只有当debug阶段正常退出（及未有ASSERT校验），npu check才会输出完整的校验日志及分析。

## 环境准备

请参考[快速入门](00_quick_start.md)完成环境准备

## 使用方法

基于Ascend C编程语言开发的算子通过[cpu_debug](01_cpu_debug.md)在CPU域执行时，npu check工具会同步对算子实现进行的检查，算子的执行过程和检索到的Error以*_npuchk.log文件的形式保存在CPU域算子可执行文件执行路径下npuchk文件夹内。通过执行以下命令，一键式生成检查结果。

  ``` bash
  # 未指定log文件，自动在当前路径下搜索log文件，其中git_clone_path为本代码仓克隆路径
  python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py

  # 指定log文件
  python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py npuchk/xxx_npuchk.log
  ```

- 检测到Error：命令行执行完毕后，失败结果打屏。例如错误码ErrorRead3及相关失败信息如下： 

  ``` bash
  [V] [ErrorRead3] on read 0x7f328c11b010 0x800B
  Rule：读取越界，长度超出经Ascend C框架的alloc_buf申请实际有效的数据（开始/结尾）
  ### vadd((__ubuf__ half*)7f328c11b810, (__ubuf__ half*)0xf328c11b010, (__ubuf__*)0x7f328c11b410, (uint8_t)1, (uint8_t)1, (uint8_t)1, (uint8_t)1, (uint8_t)8, (uint8_t)8, (uint8_t)8);

  ---------------------- ERROR STATISTICS ----------------------
  1， ErrorRead3，读取越界，长度超出经Ascend C框架的alloc_buf申请实际有效的数据（开始/结尾）
  ```

- 未检测到Error：命令执行完毕，无打屏。

若检测到Error，可在log中查看详细的执行过程。根据日志信息，划分为以下几个功能点。

### 异常检测

npu check对内存读写、指令同步、Tensor操作的合法性进行检测，常见的失败类型及对应字段如下，

- **ErrorRead1:**
    非法内存读取数据：整段内存未经过Ascend C框架的AllocTensor申请或已被FreeTensor。
- **ErrorRead2:**
    [可疑问题]读取无效数据；读取的内存部分/全部从未被写过，读取的数据可能是无效数据。
- **ErrorRead3:**
    读取越界，长度超出Ascend C框架的AllocTensor申请实际有效的数据（开始/结尾）。
- **ErrorRead4:**
    读取地址非32字节对齐。
- **ErrorWrite1:**
    非法内存写入数据，未经过Ascend C框架的AllocTensor申请或已被FreeTensor。
- **ErrorWrite2:**
    写入越界，长度超出经Ascend C框架的AllocTensor申请实际有效的数据（开始/结尾）。
- **ErrorWrite3:**
    [可疑问题]重复写入，前一次写入的内存没有被取走，重复写入。
- **ErrorWrite4:**
    写入地址非32字节对齐。
- **ErrorSync1:**
    写入存在同步问题，pipe内缺少pipe barrier或pipe间缺少set/wait。
- **ErrorSync2:**
    读取存在同步问题，pipe内缺少pipe barrier或pipe间缺少set/wait。
- **ErrorSync3:**
    set/wait使用不配对，缺少set或者wait。
- **ErrorSync4:**
    出现set/wait的eventID重复，比如mte2:set0/set0，vector:set0/wait0。
- **ErrorLeak:**
    内存泄漏，存在申请内存未释放问题。
- **ErrorFree:**
    内存重复释放，调用free_buf释放过，再次调用free_buf。
- **ErrorBuffer0:**
    tensor内存未使用Ascend C框架的InitBuffer进行初始化。
- **ErrorBuffer1:**
    tensor的que类型与初始化时不一致。
- **ErrorBuffer2:**
    VECIN/VECOUT/VECCALC的操作不合规。
- **ErrorBuffer3:**
    tensor的操作内存不合法，可能原因：内存未分配/内存越界。
- **ErrorBuffer4:**
    TBufPool资源池未使用Ascend C框架的InitBufPool接口初始化。

### EnQue/DeQue错误场景检查

对于VECIN/VECOUT/VECCALC类型的Tensor，判断Tensor出现在搬运/计算指令时是否处于正确的状态，以保证同步的正确性，对于异常的状态，会在日志中记录。

### GM内存多核踩踏检查

基于GM全局内存的管理机制，记录每个核操作的GM地址范围，发现多核写入地址范围有重叠的情况，记录错误；支持Atomic add场景下，对于重叠地址不记录错误。


## 使用示例

下面以[add](https://gitcode.com/cann/asc-devkit/blob/master/examples/01_utilities/03_cpudebug/add.cpp)为示例，介绍在调用CPU调测API并使用gdb/printf对算子核函数进行调试之后，开发者可以基于生成的log文件使用npu check工具检查Kernel源码的实现逻辑。

**步骤1**:构造错误用例

在add_custom代码的CopyIn函数中加入如下FreeTensor操作。

``` cpp
AscendC::LocalTensor<half> xLocal = inQueueX.AllocTensor<half>();
AscendC::LocalTensor<half> yLocal = inQueueY.AllocTensor<half>();
// 此处增加以下一行代码来构造错误示例
inQueueX.FreeTensor(xLocal);
// 剩余代码保持不变
AscendC::DataCopy(xLocal, xGm[progress * TILE_LENGTH], TILE_LENGTH);
AscendC::DataCopy(yLocal, yGm[progress * TILE_LENGTH], TILE_LENGTH);
inQueueX.EnQue(xLocal);
inQueueY.EnQue(yLocal);
```

在这里进行FreeTensor会导致非法内存写入数据。

**步骤2**:使用cpu debug生成log文件

参考[cpu_debug](01_cpu_debug.md)执行以下命令编译生成CPU域的算子可执行文件，add_custom_x_x_npuchk.log文件保存在执行路径下npuchk文件夹中。

```bash
set -e && rm -rf build out && mkdir -p build
cmake -B build -DCMAKE_INSTALL_PREFIX=./ -DSOC_VERSION=${SOC_VERSION}
cmake --build build -j
cmake --install build
rm -f add
cp ./build/add ./
python3 scripts/gen_data.py
(
  export LD_LIBRARY_PATH=$(pwd)/out/lib:$(pwd)/out/lib64:${ASCEND_INSTALL_PATH}/lib64:$LD_LIBRARY_PATH
  ./add | tee $file_path
)
python3 scripts/verify_result.py output_z.bin golden.bin
```

**步骤3**:找到对应的log文件进行检查

由于用例为多核用例，每个核都会生成一个相应的log文件，以0核为例，生成log文件为add_custom_0_0_vec_npuchk.log，执行如下命令进行检查。

``` shell
python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py npuchk/add_custom_0_0_vec_npuchk.log
```

  - 若不指定xxx_npuchk.log，脚本将会自动检索路径下的以“_npuchk.log”为后缀的文件进行检查。

此时查看log文件可以看到执行时npu check日志记录的堆栈信息。

**步骤4**:根据打屏信息判断错误类型

示例用例出现错误，会出现如下信息。

``` shell
----------------------ERROR STATISTICS----------------------
1，ErrorBuffer2，VECIN/VECOUT/VECCALC的操作不合规
1，ErrorWrite1，非法内存写入数据：未经过Ascend C框架的alloc_buf申请或已经free
```

此时可根据上方异常检测部分判断错误类型。