# NDDMA 连续状态与合法边界

## API 依据

参考 devkit 的 `DataCopy_GMToUB_NDDMA.md` 及其链接的
`examples/01_simd_cpp_api/03_basic_api/00_data_movement/data_copy_gm2ub_nddma/multidimensional_data_movement.asc`。
使用 Basic API `NdDmaLoopInfo`、`NdDmaParams`、`NdDmaDci` 和 `DataCopy`，目标为 dav-3510。
每次搬运前刷新 NDDMA Cache，复用 UB 前执行 `PipeBarrier<PIPE_ALL>`。

## 用例

源码统一为 `nddma_scenarios_memory_case.asc`，每个名称均有 `_valid` 和 `_oob` 两个 target。

| 名称前缀 | profile | 合法分配字节 | 越界分配字节 | 检查内容 |
| --- | --- | --- | --- | --- |
| nddma_scenarios_state_axis0 | 6 | 343 | 342 | loop0 stride 1 → 2 → 1 |
| nddma_scenarios_state_axis1 | 7 | 346 | 345 | loop1 stride 4 → 8 → 4 |
| nddma_scenarios_state_axis2 | 8 | 358 | 357 | loop2 stride 16 → 32 → 16 |
| nddma_scenarios_state_axis3 | 9 | 406 | 405 | loop3 stride 64 → 128 → 64 |
| nddma_scenarios_state_axis4 | 10 | 598 | 597 | loop4 stride 256 → 512 → 256 |
| nddma_scenarios_singleton_max_stride | 11 | 2 | 1 | 单次循环轴的最大合法源/目的 stride |
| nddma_scenarios_zero_stride_5d | 12 | 2 | 1 | 四个活动轴的零源 stride 广播 |

profile 6～10 在同一 block、同一 launch 中连续搬运三次。五轴 loopSize 均为 2，
初始源 stride 为 `{1,4,16,64,256}`，目的 stride 为 `{1,2,4,8,16}`。
首尾搬运最大访问偏移为 341；中间搬运仅扩大所选轴的源 stride。
正常版要求 `device_operations=3 errors=0 warnings=0`；异常版要求
`device_operations=3 errors=1 warnings=0`，仅中间搬运的最后一个字节越界。
这样能同时检测未采用新 stride 导致的漏报，以及恢复后仍采用扩大 stride 导致的误报。

profile 11 的 loopSize 为 `{2,1,1,1,1}`，后四轴源 stride 为 `2^40-1`、
目的 stride 为 `2^20-1`。单次循环轴不增加访问范围，实际只读取两个字节。
它验证合法字段上限与 singleton 的组合，不声明覆盖实际跨度为 1TB 的搬运。

profile 12 的 loopSize 均为 2，源 stride 为 `{1,0,0,0,0}`，目的 stride 为
`{1,2,4,8,16}`，将两个源字节广播到 32 个目的字节。

所有判定使用客户可见的 GM 越界诊断、summary、CLI 结果和完整 session。
不检查内部 raw、register 或 CBData 日志。转换失败仍沿用跳过及内部日志行为。
这些用例不穷举 loopSize、padding 和所有数据类型，也不制造非法 raw 算术溢出。

## 执行

加载 CANN 环境并确保 `npu-check` 可用，在本目录执行：

```bash
export NPU_CHECK_E2E_REUSE_EXISTING_BUILD=1
bash check_memory_access_end_to_end.sh nddma_scenarios_state_axis0_valid
bash check_memory_access_end_to_end.sh nddma_scenarios_state_axis0_oob
for name in $(bash check_memory_access_end_to_end.sh --list | grep -E 'nddma_scenarios_(state_axis|singleton_max_stride|zero_stride_5d)'); do
    bash check_memory_access_end_to_end.sh "$name" || break
done
```

日志保存在 `build/memory_access_logs/<用例名>.log`。

单点编译并检查 axis0 越界用例：

```bash
bisheng -xasc nddma_scenarios_memory_case.asc --npu-arch=dav-3510 -g \
    -DNDDMA_SCENARIO_PROFILE=6 -DNDDMA_SCENARIO_SOURCE_BYTES=342 \
    -o /tmp/nddma_state_axis0_oob -lacl_rt
npu-check --tool memcheck -- /tmp/nddma_state_axis0_oob
```

## 实机结果

2026-09-07，在 dav-3510 的 `rj` Docker、CANN 9.2.0 环境中验证新增 14 例：
14/14 PASS。使用该环境已安装的 `npu-check`，未修改 sanitizer 实现。
7 个正常用例均为 `errors=0 warnings=0`，7 个异常用例均为 `errors=1 warnings=0`；
所有用例均为 `DEVICE_SYNC_RESULT=0`、完整 session 和 `child_exit=0`。
五个轴的异常报告均定位到中间的第二次 `DataCopy`；首尾搬运没有报告越界。
另外回归 `nddma_scenarios_broadcast_b8_valid` 和 `nddma_scenarios_multi_block_b8_oob`，2/2 PASS。

本次新增验证不等于重新执行了全部 244 例，也不等于穷举全部合法算术边界。
