# NPU Compute

NPU Compute 是一个命令行性能分析原型工具，由注入式采集库和基于 ACLPTI
实现的 Kernel 重放功能组成。

产品调用链如下：

```text
npu-compute
  -> 在当前工作目录创建独立的采集数据目录
  -> 使用 ACL_API_INJECTION=libnpu-compute.so 和
     NPU_COMPUTE_OUTPUT=<data-directory> 启动目标程序
  -> 目标程序初始化 Runtime
  -> prof_api 加载 libnpu-compute.so
  -> acltoolInitialize
  -> ACLPTI 订阅流程初始化 Hook 和重放依赖
  -> 启用 Runtime 回调并配置采集 Section
  -> Hook Runtime 调用、维护影子内存并重放 Kernel
  -> 目标程序首次成功的目标 Kernel 启动 Runtime EXIT 回调触发 HardwareInfo 采集
  -> Msprof 原始数据回调和内部重放数据生命周期处理
  -> CLI 通过 waitpid 观察到目标程序成功退出
  -> CLI 校验 HardwareInfo.jsonl，并递归打包采集数据目录
  -> CLI 以原子方式发布 report_<epoch_ms>_<random_id>.npu-rep 报告
```

## 组件

| 组件 | 集成产物 | 职责 |
| --- | --- | --- |
| NPU Compute CLI | `npu-compute` | 校验参数、启动并监控目标程序、打包采集文件以及解包导入的报告 |
| NPU Compute 库 | `libnpu-compute.so` | 导出 `acltoolInitialize`/`acltoolShutdown`、配置 ACLPTI、采集 HardwareInfo 并写入性能分析 CSV 文件 |
| ACLPTI | `libacl_pti.so` | 将 Section 展开为 PMU 事件、维护影子内存并重放 Kernel |
| CANN Prof API | `libprofapi.so` | 注册 Runtime 表、加载注入库并接收 profiling 数据 |
| Injection Hook | `libacl_tool_injection.so` | 注册 API replacement 并提供原始 Runtime 函数 |
| PTI 数据模块 | 编译到 `libacl_pti.so` | 解码性能分析数据块、聚合 Task/PMU 数据行，并在数据排空后通知已注册的关闭处理函数 |
| CANN Runtime | `libacl_rt.so`、`libruntime.so` | 提供 Runtime API 和 API injection 分发 |

生产构建使用所配置 CANN 软件包中的 Prof API 和 Runtime 库。仓库内的 Prof API
和 Runtime 实现仅用于单元测试。PTI 重放数据实现被编译到 `libacl_pti.so` 中，
NPU Compute 通过该库使用此实现。

ACLPTI 的私有 C++ 实现位于
`npu_compute::aclpti::{callback,activity,data,profiling,replacement}`
命名空间下。只有跨域的初始化模块保留在 `npu_compute::aclpti` 中。
公共 `aclptiXxx` API 保持在全局命名空间中，文件内辅助函数保持在匿名命名空间中。

## 支持的 Section

CLI 当前提供以下由 CSV 写入器支持的 Section：

```text
PipeUtilization
Memory
MemoryL0
MemoryUB
L2Cache
```

`HardwareInfo` 不是 Section。每条采集命令默认启用 HardwareInfo 采集，且该采集
独立于所选的 PMU Section。目标程序首次成功的目标 Kernel 启动 Runtime EXIT 回调
会启动唯一一次采集。重复的 Section 会被去重，同时保留首次出现的顺序。

## CLI

```text
npu-compute [options] [program] [program-arguments]
```

公开选项：

```text
-h, --help
    --section <id>
    --list-sections
    --replay-mode kernel
-i, --import <repo>
-o, --export <repo>
```

执行采集时必须至少指定一个 `--section` 和一个目标程序。目标程序之后的参数会
原样传递给应用。CLI 作为父进程运行，将 `SIGINT`、`SIGTERM` 和 `SIGHUP` 转发
给目标进程组，回收子进程，并保留正常退出状态或由信号产生的退出状态。每条采集
命令都会在执行命令时的当前工作目录中获得唯一的采集数据目录。如果应用成功退出，但
`HardwareInfo.jsonl` 缺失或不是普通文件，CLI 会输出采集数据目录路径并返回采集错误码 3。

对于采集命令，`--export` 用于指定报告目标路径。以 `.npu-rep` 结尾时，该路径
将作为报告文件的完整目标路径；如果指定已存在的目录，则在该目录中生成自动命名的报告。
未指定 `--export` 时，CLI 在当前目录中创建报告：

```text
report_<epoch_ms>_<8-lowercase-hex-digits>.npu-rep
```

对于导入命令，`--export` 用于指定已有目录。每次导入都会在该目录中创建唯一的
结果子目录。未指定 `--export` 时，CLI 在当前目录中创建唯一的结果子目录。结果目录
名称格式为 `npu-compute-import-<毫秒时间戳>-<进程 ID>-<随机后缀>`。

示例：

```bash
# 采集：在当前目录中发布自动命名的报告。
npu-compute --section PipeUtilization ./application

# 采集：发布到指定的报告文件路径。
npu-compute --section PipeUtilization \
  --export result.npu-rep ./application

# 导入：在当前目录中创建唯一结果目录。
npu-compute --import result.npu-rep

# 导入：在指定已有目录中创建唯一结果目录。
mkdir restored-results
npu-compute --import result.npu-rep --export restored-results
```

## 报告打包与导入

目标程序成功退出后，CLI 会校验采集输出并递归打包采集数据目录。支持的叶子文件会
保留原始文件名和字节内容。每个子目录会被编码为一个名为 `<directory>.npu.rep`、
`type=NpuRep` 的条目；其载荷是一个完整的嵌套 REP，偏移量从自身字节空间的 0
开始计算。该规则会递归应用，不设固定深度限制。

打包器接受以 `.json`、`.jsonl`、`.csv`、`.sqlite3`、`.pb` 和 `.protobuf`
为后缀的采集文件。打包时排除 `.hardware_info.lock`，拒绝残留的临时文件和
符号链接，并在读取载荷前校验 JSONL 和 CSV 的完整性。

报告写入器会在报告目标目录中创建临时文件，写入并同步完整的 REP，再回读校验
字节内容和布局，最后通过不覆盖已有目标的重命名操作发布报告并同步目标目录。已有报告
文件不会被覆盖。采集成功后会输出以下两个保留的诊断路径：

```text
npu-compute: data-directory=<absolute-data-directory>
npu-compute: report=<absolute-report-path>
```

导入操作不会启动应用，也不会初始化 Runtime、ProfAPI、ACLPTI 或
`libnpu-compute.so`。CLI 会校验外层 REP 和每个嵌套 REP，然后在不改变字节内容
的情况下恢复叶子文件。名为 `<name>.npu.rep` 或 `<name>.rep` 的嵌套条目会
恢复为 `<name>` 目录。

导入操作首先在结果子目录的父目录中创建私有临时目录。叶子文件以独占创建方式创建，
目标已存在时失败，同时不跟随符号链接，并在关闭前同步文件。完整目录随后通过不覆盖
已有目标的重命名操作发布为结果子目录。已有文件不会被覆盖，导入失败时不会发布
不完整的结果目录。导入成功后输出：

```text
npu-compute: unpacked=<absolute-output-directory>
```

## HardwareInfo 采集

HardwareInfo 采集默认启用，CLI 不提供对应开关。不要将 `HardwareInfo` 传递给
`--section`。

执行 `acltoolInitialize` 时，`libnpu-compute.so` 会订阅 ACLPTI Runtime 回调，
并依次启用 `aclrtLaunchKernel` 和 `aclrtLaunchKernelWithHostArgs`。ACLPTI 可能
同时上报 ENTER 和 EXIT 事件。该库只接受上述两个 API 中任意一个成功的 EXIT
事件。首个被接受的事件所在回调线程依次采集主机信息和 Device 信息，然后序列化
并以原子方式发布 `<data-directory>/HardwareInfo.jsonl`。该回调在发布完成或采集
进入失败状态后返回。

同一目标进程中，如果其他线程在采集期间同时进入被接受的回调，它们等待本次采集
进入完成或失败状态后返回，不会重复采集。关闭时先禁用已启用的回调；如果采集正在
执行，则等待采集进入最终状态。未收到有效 Kernel EXIT 回调时不生成
`HardwareInfo.jsonl`。成功发布的文件按以下顺序包含五个 JSON 对象：

```jsonl
{"category":"Host Info","cpu physical count":0,"cpu logical count":0,"memory total size(MB)":0,"disk total size(GB)":0}
{"category":"Device Info","npu count":0,"chip info":"","arch info":""}
{"category":"CPU Information","control cpu count":0,"ai cpu count":0,"ai cpu frequency(MHZ)":0}
{"category":"AI Core Information","ai core count":0,"ai cube count":0,"ai vector count":0,"ai cube frequency(MHZ)":0,"ai vector frequency(MHZ)":0}
{"category":"Memory Information","hbm total(MB)":0,"hbm used(MB)":0,"hbm frequency(MHZ)":0}
```

当前实现支持单卡采集，并查询 Device。单个设备字段查询失败时，会以
`[libnpu-compute] HardwareInfo:` 为前缀向 `stderr` 输出错误，并将该字段保留为
默认值。初始化、主机信息采集、序列化或发布失败时也会输出诊断信息；如果最终没有
发布普通文件，CLI 会返回采集错误码 3，并输出保留的采集数据目录。

## 构建

构建生产产物前需要加载目标 CANN 软件包环境。当前测试环境可使用：

```bash
source /home/chenning/AscendEnv/test_profiling/cann-9.2.0/set_env.sh
cmake -S npu_compute -B /tmp/asc_tools_npu_compute_product \
  -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build /tmp/asc_tools_npu_compute_product -j2
```

CANN 架构软件包从 `ASCEND_HOME_PATH` 对应的头文件位置推导；只有需要覆盖该环境时
才显式设置 `NPUCOMPUTE_CANN_ROOT`。`CMAKE_ASC_ARCHITECTURES` 应与目标 NPU
匹配（`dav-2201` 或 `dav-3510`）。构建会生成 `libnpu-compute.so`、
`libacl_pti.so`、`libacl_tool_injection.so` 和 `npu-compute` CLI，并链接 CANN
提供的 Prof API 和 Runtime，而不是仓库中的测试桩。

单元测试会自动选择仓库内的 Prof API 和 Runtime 测试桩。Runtime 测试桩只覆盖
injection 声明，其 `#include_next` 仍需要从已加载的环境中获得 CANN 头文件：

```bash
source /home/chenning/AscendEnv/test_profiling/cann-9.2.0/set_env.sh
cmake -S npu_compute -B /tmp/asc_tools_npu_compute_ut \
  -DNPU_COMPUTE_BUILD_TESTS=ON
cmake --build /tmp/asc_tools_npu_compute_ut -j2
LD_LIBRARY_PATH=/tmp/asc_tools_npu_compute_ut/bin:${LD_LIBRARY_PATH} \
  ctest --test-dir /tmp/asc_tools_npu_compute_ut --output-on-failure
```

asc-tools 顶层构建提供 NPU Compute 测试开关：

```bash
cmake -S . -B build \
  -DASC_TOOLS_BUILD_NPU_COMPUTE=ON \
  -DNPU_COMPUTE_BUILD_TESTS=ON
```

## 安装

在仓库根目录构建默认的 asc-tools run 包：

```bash
bash build.sh --pkg
```

将生成的安装包安装到默认 CANN 路径：

```bash
./build_out/cann-asc-tools_<version>_linux-<arch>.run --full --pylocal
```

在 asc-tools run 包中，架构相关文件位于 CANN 架构目录下。此处 `<arch>` 为
`uname -m` 的输出值：

```text
<arch>-linux/bin/npu-compute
<arch>-linux/lib64/libnpu-compute.so
<arch>-linux/lib64/libacl_pti.so
<arch>-linux/lib64/libacl_tool_injection.so
<arch>-linux/include/aclpti/*.h
```

安装期间，CANN 通过顶层的 `bin`、`lib64` 和 `include` 符号链接公开架构相关
目录。执行 `source <install-root>/cann/set_env.sh` 后，对外安装路径如下：

```text
$ASCEND_HOME_PATH/bin/npu-compute
$ASCEND_HOME_PATH/lib64/libnpu-compute.so
$ASCEND_HOME_PATH/lib64/libacl_pti.so
$ASCEND_HOME_PATH/lib64/libacl_tool_injection.so
$ASCEND_HOME_PATH/include/aclpti/*.h
```

匹配的 CANN Runtime 基础包会在同一公共 `lib64` 路径下提供 `libprofapi.so` 和
`libacl_rt.so`。ProfAPI 桩和 Runtime 桩保留为构建目录中的测试产物，不会由
asc-tools run 包安装。

## 重放约定

ACLPTI 会为每次成功的设备内存分配创建同等大小的影子内存。被 Hook 的 H2D/D2D
内存复制和内存设置操作会同步更新影子内存。释放内存时会同时释放原始内存和影子
内存。

ACLPTI 为内存分配、释放、复制、设置和 Kernel 启动注册完整的替换函数。每个替换函数
通过 `acltoolGetOriginalRuntimeApi` 调用原始 Runtime 函数；只有在原始调用成功后，
ACLPTI 才会更新影子状态或启动重放。

对于每次成功的原始 Kernel 启动 API 调用，ACLPTI 会将选定的 PMU 事件拆分为多轮，
每轮最多包含十个值。每轮依次恢复影子状态、准备内部重放记录、启动 Msprof、调用
对应的原始启动 API、同步 Stream、停止 Msprof、记录采集重放状态并释放本轮重放。
最后一轮成功完成后，应用可见缓冲区会保持 Kernel 执行后的状态；所有轮次完成后，系统
会关闭 PTI 数据模块。发生错误时，也会在 `ReplayKernel` 返回前关闭数据模块，同时
保留原始错误码。

当前初始实现仅支持每个进程重放一次 Kernel 启动。数据模块关闭后，后续 Kernel 启动无法
再次触发重放。数据模块会异步解码和聚合性能分析记录，但系统当前不会跟踪已初始化
子范围、为多 Kernel 序列创建快照、持久化性能分析输出、串行化并发采集，也不会补偿
后续重放轮次失败造成的影响。
