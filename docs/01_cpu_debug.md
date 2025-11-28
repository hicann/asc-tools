# cpu debug

## 概述

在算子部署到NPU上之前，cpu debug工具帮助用户在CPU上进行功能和精度的基本验证。基本方案如下：开发者通过调用Ascend C编程语言编写算子Kernel侧源码，Kernel侧源码通过通用的GCC编译器进行编译，编译生成通用的CPU域的二进制，可以通过gdb/printf等调试手段进行调试。

## 环境准备

请参考[快速入门](00_quick_start.md)完成环境准备。

## 使用方法

- 使用CPU调测API编写程序。

- 编译CPU调试程序，基于CPU调试库完成算子的编译。

- 使用gbd、printf打印等方法进行CPU侧调试。

## 使用示例

下面以[add](https://gitcode.com/cann/asc-devkit/blob/master/examples/01_utilities/03_cpudebug/add.cpp)为示例，介绍如何调用CPU调测API并使用gdb/printf对算子核函数进行调试，开发者可以基于用例所包含的一键式脚本适配自定义算子用例进行编译、执行。

**步骤1**：头文件适配

分别包含cpu侧和npu侧所需要的头文件，通过ASCENDC_CPU_DEBUG宏区分CPU和NPU侧的头文件。
``` c
#include "data_utils.h"
#ifndef ASCENDC_CPU_DEBUG
#include "acl/acl.h"
#else
#include "tikicpulib.h"
extern "C" __global__ __aicore__ void add_custom(GM_ADDR x, GM_ADDR y, GM_ADDR z); // 核函数声明
#endif
```

**步骤2**：接口适配

使用CPU调测API GmAlloc、ICPU_RUN_KF、GmFree（接口说明请参考[Ascend C API中的调试接口](https://hiascend.com/document/redirect/CannCommunityAscendCApi)）编写调试程序。
``` c
int32_t main(int32_t argc, char* argv[])
{
    uint32_t blockDim = 8;
    size_t inputByteSize = 8 * 2048 * sizeof(uint16_t);
    size_t outputByteSize = 8 * 2048 * sizeof(uint16_t);

    // 使用GmAlloc分配共享内存，并进行数据初始化
    uint8_t* x = (uint8_t*)AscendC::GmAlloc(inputByteSize);
    uint8_t* y = (uint8_t*)AscendC::GmAlloc(inputByteSize);
    uint8_t* z = (uint8_t*)AscendC::GmAlloc(outputByteSize);

    ReadFile("./input/input_x.bin", inputByteSize, x, inputByteSize);
    ReadFile("./input/input_y.bin", inputByteSize, x, inputByteSize);
    // 矢量算子需要设置内核模式为AIV模式
    AscendC::SetKernelMode(KernelMode::AIV_MODE);
    // 调用ICPU_RUN_KF调测宏，完成核函数CPU侧的调用
    ICPU_RUN_KF(add_custom, blockDim, x, y, z);
    // 输出数据写出
    WriteFile("./output/output_z.bin", z, outputByteSize);
    // 调用GmFree释放申请的内存
    AscendC::GmFree((void *)x);
    AscendC::GmFree((void *)y);
    AscendC::GmFree((void *)z);
}
```

**步骤3**：编译CPU调试程序

参考[CPU Debug直调样例说明](https://gitcode.com/cann/asc-devkit/blob/master/examples/01_utilities/03_cpudebug/README.md)，配置环境变量后执行以下命令编译生成CPU域的算子可执行文件。

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

**步骤4**：printf命令打印

在代码中直接编写printf(...)来观察数值的输出。样例代码如下：

```c
printf("xLocal size: %d\n", xLocal.GetSize())
printf("tileLength: %d\n", tileLength)
```

示例中在Compute函数加入打印代码如下，执行指令后即可输出xLocal与yLocal的值。
```cpp
AscendC::LocalTensor<half> xLocal = inQueueX.DeQue<half>();
AscendC::LocalTensor<half> yLocal = inQueueY.DeQue<half>();
//加入打印操作
printf("xLocal[0]: %f \n", static_cast<float>(xLocal(0)));
printf("yLocal[0]: %f \n", static_cast<float>(yLocal(0)));
//其余代码保持不变
AscendC::LocalTensor<half> zLocal = outQueueZ.AllocTensor<half>();
AscendC::Add(zLocal, xLocal, yLocal, TILE_LENGTH);
outQueueZ.EnQue<half>(zLocal);
inQueueX.FreeTensor(xLocal);
inQueueY.FreeTensor(yLocal);
```