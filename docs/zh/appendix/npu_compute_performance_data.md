# npu-compute性能数据

## ArithmeticUtilization（Cube及Vector指令占比和数量）

Cube及Vector类型指令的活跃周期占比和Cube指令条数保存在ArithmeticUtilization.csv中，可用于查看各类计算指令的活跃程度和Cube指令的执行数量。详情介绍请参见下表中的字段说明。

NA表示当前Core不适用或所需数据不可用，不表示数值零。

| 字段名 | 字段解释 |
|---|---|
| `block_id` | 当前数据行所属的逻辑Block编号。 |
| `sub_block_id` | 当前Block内的Core类型和Sub-block编号。AI Cube Core显示为cubeN，AI Vector Core显示为vectorN，其中N为Sub-block编号，不是物理核编号。 |
| `aic_time(us)` | 当前数据行对应AI Cube Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aic_total_cycles` | 当前数据行对应AI Cube Core执行的总周期数。 |
| `aic_cube_ratio` | AIC Cube 活跃周期占 AIC 总周期的比值。 |
| `aic_cube_fp_ratio` | AIC Cube 浮点指令活跃周期占 AIC 总周期的比值。 |
| `aic_cube_int_ratio` | AIC Cube 整数指令活跃周期占 AIC 总周期的比值。 |
| `aic_cube_total_instr_number` | AIC Cube执行的总指令条数。 |
| `aic_cube_fp_instr_number` | AIC Cube执行的浮点指令条数。 |
| `aic_cube_int_instr_number` | AIC Cube执行的整数指令条数。 |
| `aiv_time(us)` | 当前数据行对应AI Vector Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aiv_total_cycles` | 当前数据行对应AI Vector Core执行的总周期数。 |
| `aiv_vec_ratio` | AIV Vector 活跃周期占 AIV 总周期的比值。 |
| `aiv_vec_vf_ratio` | AIV Vector VF 指令活跃周期占 AIV 总周期的比值。 |
| `aiv_vec_sfu_ratio` | AIV Vector SFU 指令活跃周期占 AIV 总周期的比值。 |
| `aiv_vec_simt_vf_ratio` | AIV SIMT VF 指令活跃周期占 AIV 总周期的比值。 |

## L2Cache（L2 Cache访问计数和命中率）

L2 Cache读写计数和命中率保存在L2Cache.csv中，可用于查看AI Cube Core和AI Vector Core访问近端、远端L2 Cache时的命中、未命中及驱逐情况。详情介绍请参见下表中的字段说明。

NA表示当前Core不适用或所需数据不可用，不表示数值零。

| 字段名 | 字段解释 |
|---|---|
| `block_id` | 当前数据行所属的逻辑Block编号。 |
| `sub_block_id` | 当前Block内的Core类型和Sub-block编号。AI Cube Core显示为cubeN，AI Vector Core显示为vectorN，其中N为Sub-block编号，不是物理核编号。 |
| `aic_time(us)` | 当前数据行对应AI Cube Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aic_total_cycles` | 当前数据行对应AI Cube Core执行的总周期数。 |
| `aic_read_close_hit` | AIC读取近端L2 Cache时的命中次数。 |
| `aic_read_close_miss` | AIC读取近端L2 Cache时的未命中次数。 |
| `aic_read_close_victim` | AIC读取近端L2 Cache时记录的驱逐次数。 |
| `aic_read_far_hit` | AIC读取远端L2 Cache时的命中次数。 |
| `aic_read_far_miss` | AIC读取远端L2 Cache时的未命中次数。 |
| `aic_read_far_victim` | AIC读取远端L2 Cache时记录的驱逐次数。 |
| `aic_read_hit_rate(%)` | AIC读取L2 Cache的命中率，以百分比表示。命中次数包括近端和远端命中，总次数包括近端和远端的命中、未命中、驱逐次数；计数齐全且总次数为零时显示为0。 |
| `aic_write_close_hit` | AIC写入近端L2 Cache时的命中次数。 |
| `aic_write_close_miss` | AIC写入近端L2 Cache时的未命中次数。 |
| `aic_write_close_victim` | AIC写入近端L2 Cache时记录的驱逐次数。 |
| `aic_write_far_hit` | AIC写入远端L2 Cache时的命中次数。 |
| `aic_write_far_miss` | AIC写入远端L2 Cache时的未命中次数。 |
| `aic_write_far_victim` | AIC写入远端L2 Cache时记录的驱逐次数。 |
| `aic_write_hit_rate(%)` | AIC写入L2 Cache的命中率，以百分比表示。命中次数包括近端和远端命中，总次数包括近端和远端的命中、未命中、驱逐次数；计数齐全且总次数为零时显示为0。 |
| `aiv_time(us)` | 当前数据行对应AI Vector Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aiv_total_cycles` | 当前数据行对应AI Vector Core执行的总周期数。 |
| `aiv_read_close_hit` | AIV读取近端L2 Cache时的命中次数。 |
| `aiv_read_close_miss` | AIV读取近端L2 Cache时的未命中次数。 |
| `aiv_read_close_victim` | AIV读取近端L2 Cache时记录的驱逐次数。 |
| `aiv_read_far_hit` | AIV读取远端L2 Cache时的命中次数。 |
| `aiv_read_far_miss` | AIV读取远端L2 Cache时的未命中次数。 |
| `aiv_read_far_victim` | AIV读取远端L2 Cache时记录的驱逐次数。 |
| `aiv_read_hit_rate(%)` | AIV读取L2 Cache的命中率，以百分比表示。命中次数包括近端和远端命中，总次数包括近端和远端的命中、未命中、驱逐次数；计数齐全且总次数为零时显示为0。 |
| `aiv_write_close_hit` | AIV写入近端L2 Cache时的命中次数。 |
| `aiv_write_close_miss` | AIV写入近端L2 Cache时的未命中次数。 |
| `aiv_write_close_victim` | AIV写入近端L2 Cache时记录的驱逐次数。 |
| `aiv_write_far_hit` | AIV写入远端L2 Cache时的命中次数。 |
| `aiv_write_far_miss` | AIV写入远端L2 Cache时的未命中次数。 |
| `aiv_write_far_victim` | AIV写入远端L2 Cache时记录的驱逐次数。 |
| `aiv_write_hit_rate(%)` | AIV写入L2 Cache的命中率，以百分比表示。命中次数包括近端和远端命中，总次数包括近端和远端的命中、未命中、驱逐次数；计数齐全且总次数为零时显示为0。 |

## Memory（内存读写带宽和数据量）

主存、L1和UB相关的读写带宽、数据量及搬运指令信息保存在Memory.csv中，可用于查看主存读写和各数据通路的搬运情况。详情介绍请参见下表中的字段说明。

带宽按搬运数据量和执行时间计算，单位为GB/s。混合算子有有效Task耗时时使用该耗时，否则使用当前Core的执行时间。

NA表示当前Core不适用或所需数据不可用，不表示数值零。

| 字段名 | 字段解释 |
|---|---|
| `block_id` | 当前数据行所属的逻辑Block编号。 |
| `sub_block_id` | 当前Block内的Core类型和Sub-block编号。AI Cube Core显示为cubeN，AI Vector Core显示为vectorN，其中N为Sub-block编号，不是物理核编号。 |
| `aic_time(us)` | 当前数据行对应AI Cube Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aic_total_cycles` | 当前数据行对应AI Cube Core执行的总周期数。 |
| `aic_l1_read_bw(GB/s)` | AIC 从 L1 读取数据的平均带宽，单位为GB/s。 |
| `aic_l1_write_bw(GB/s)` | AIC 向 L1 写入数据的平均带宽，单位为GB/s。 |
| `aic_main_mem_read_bw(GB/s)` | AIC 主存读取平均带宽，单位为GB/s。 |
| `aic_main_mem_write_bw(GB/s)` | AIC 主存写入平均带宽，单位为GB/s。 |
| `aic_mte1_instructions` | AIC执行的MTE1指令条数。 |
| `aic_mte1_ratio` | AIC MTE1 活跃周期占总周期的比值。 |
| `aic_mte2_instructions` | AIC执行的MTE2指令条数。 |
| `aic_mte2_ratio` | AIC MTE2 活跃周期占总周期的比值。 |
| `aic_mte3_instructions` | AIC执行的MTE3指令条数。 |
| `aic_mte3_ratio` | AIC MTE3 活跃周期占总周期的比值。 |
| `aiv_time(us)` | 当前数据行对应AI Vector Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aiv_total_cycles` | 当前数据行对应AI Vector Core执行的总周期数。 |
| `aiv_ub_to_gm_bw(GB/s)` | 从UB搬运到GM的带宽，单位为GB/s。当前所需插桩数据未提供，固定显示为NA。 |
| `aiv_gm_to_ub_bw(GB/s)` | AIV GM 到 UB 相关平均带宽，单位为GB/s。 |
| `aiv_main_mem_read_bw(GB/s)` | AIV 主存读取平均带宽，单位为GB/s。 |
| `aiv_main_mem_write_bw(GB/s)` | AIV 主存写入平均带宽，单位为GB/s。 |
| `aiv_mte2_instructions` | AIV执行的MTE2指令条数。 |
| `aiv_mte2_ratio` | AIV MTE2 活跃周期占总周期的比值。 |
| `aiv_mte3_instructions` | AIV执行的MTE3指令条数。 |
| `aiv_mte3_ratio` | AIV MTE3 活跃周期占总周期的比值。 |
| `read_main_memory_datas(KB)` | 当前数据行对应Core从主存读取的数据量，单位为KB。 |
| `write_main_memory_datas(KB)` | 当前数据行对应Core向主存写入的数据量，单位为KB。 |
| `GM_to_L1_datas(KB)` | 从GM搬运到L1的数据量，单位为KB。 |
| `GM_to_L1_bw_usage_rate(%)` | GM到L1通路平均带宽占配置最大带宽的百分比，最高显示为100%。 |
| `L0C_to_L1_datas(KB)` | 从L0C搬运到L1的数据量，单位为KB。 |
| `L0C_to_L1_bw_usage_rate(%)` | L0C到L1通路平均带宽占配置最大带宽的百分比，最高显示为100%。 |
| `L0C_to_GM_datas(KB)` | 从L0C搬运到GM的数据量，单位为KB。 |
| `L0C_to_GM_bw_usage_rate(%)` | L0C到GM通路平均带宽占配置最大带宽的百分比，最高显示为100%。 |
| `GM_to_UB_datas(KB)` | 从GM搬运到UB的数据量，单位为KB。 |
| `GM_to_UB_bw_usage_rate(%)` | GM到UB通路平均带宽占配置最大带宽的百分比，最高显示为100%。 |
| `UB_to_GM_datas(KB)` | 从UB搬运到GM的数据量，单位为KB。当前所需插桩数据未提供，固定显示为NA。 |
| `UB_to_GM_bw_usage_rate(%)` | UB到GM通路的带宽利用率，以百分比表示。当前所需插桩数据未提供，固定显示为NA。 |

GM到UB的数据量使用主存读取计数扣除两类DCache未命中相关读取计数后计算，扣除结果小于零时按零处理。GM到L1和L0C到GM分别使用AIC主存读取和写入计数计算。

## MemoryL0（L0读写带宽）

L0A、L0B、L0C与Cube或L1之间的读写带宽保存在MemoryL0.csv中，可用于查看各数据通路的平均带宽。详情介绍请参见下表中的字段说明。

带宽字段适用于AI Cube Core。带宽按搬运数据量和执行时间计算，单位为GB/s。混合算子有有效Task耗时时使用该耗时，否则使用当前Core的执行时间。

NA表示当前Core不适用或所需数据不可用，不表示数值零。

| 字段名 | 字段解释 |
|---|---|
| `block_id` | 当前数据行所属的逻辑Block编号。 |
| `sub_block_id` | 当前Block内的Core类型和Sub-block编号。AI Cube Core显示为cubeN，AI Vector Core显示为vectorN，其中N为Sub-block编号，不是物理核编号。 |
| `aic_time(us)` | 当前数据行对应AI Cube Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aic_total_cycles` | 当前数据行对应AI Cube Core执行的总周期数。 |
| `aic_l0a_read_bw(GB/s)` | L0A 到 Cube 方向的读取带宽，单位为GB/s。 |
| `aic_l0a_write_bw(GB/s)` | L1 到 L0A 方向的写入带宽，单位为GB/s。 |
| `aic_l0b_read_bw(GB/s)` | L0B 到 Cube 方向的读取带宽，单位为GB/s。 |
| `aic_l0b_write_bw(GB/s)` | L1 到 L0B 方向的写入带宽，单位为GB/s。 |
| `aic_l0c_read_bw_cube(GB/s)` | L0C 到 Cube 方向的读取带宽，单位为GB/s。 |
| `aic_l0c_write_bw_cube(GB/s)` | Cube 到 L0C 方向的写入带宽，单位为GB/s。 |
| `aiv_time(us)` | 当前数据行对应AI Vector Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aiv_total_cycles` | 当前数据行对应AI Vector Core执行的总周期数。 |

## MemoryUB（UB读写带宽）

UB与Vector、GM之间的读写带宽保存在MemoryUB.csv中，可用于查看Vector访问UB以及GM向UB搬入数据的平均带宽。详情介绍请参见下表中的字段说明。

带宽字段适用于AI Vector Core。带宽按搬运数据量和执行时间计算，单位为GB/s。混合算子有有效Task耗时时使用该耗时，否则使用当前Core的执行时间。

NA表示当前Core不适用或所需数据不可用，不表示数值零。

| 字段名 | 字段解释 |
|---|---|
| `block_id` | 当前数据行所属的逻辑Block编号。 |
| `sub_block_id` | 当前Block内的Core类型和Sub-block编号。AI Cube Core显示为cubeN，AI Vector Core显示为vectorN，其中N为Sub-block编号，不是物理核编号。 |
| `aic_time(us)` | 当前数据行对应AI Cube Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aic_total_cycles` | 当前数据行对应AI Cube Core执行的总周期数。 |
| `aiv_time(us)` | 当前数据行对应AI Vector Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aiv_total_cycles` | 当前数据行对应AI Vector Core执行的总周期数。 |
| `aiv_ub_read_bw_vector(GB/s)` | UB 到 Vector 方向的数据读取带宽，单位为GB/s。 |
| `aiv_ub_write_bw_vector(GB/s)` | Vector 到 UB 方向的数据写入带宽，单位为GB/s。 |
| `aiv_ub_read_bw_gm(GB/s)` | 从UB读取数据并搬往GM的带宽，单位为GB/s。当前所需插桩数据未提供，固定显示为NA。 |
| `aiv_ub_write_bw_gm(GB/s)` | 代表GM向UB写入数据的平均带宽，单位为GB/s。 |

## PipeUtilization（计算和搬运单元耗时占比）

计算和搬运单元的活跃时间、周期占比、活跃带宽及指令缓存未命中率保存在PipeUtilization.csv中。可用于查看各流水的活跃时间和周期占比。详情介绍请参见下表中的字段说明。

活跃带宽使用对应流水实际活跃时间计算。部分带宽还依赖Memory或MemoryUB提供的数据，单独采集本项时可能显示为NA。

NA表示当前Core不适用或所需数据不可用，不表示数值零。

| 字段名 | 字段解释 |
|---|---|
| `block_id` | 当前数据行所属的逻辑Block编号。 |
| `sub_block_id` | 当前Block内的Core类型和Sub-block编号。AI Cube Core显示为cubeN，AI Vector Core显示为vectorN，其中N为Sub-block编号，不是物理核编号。 |
| `aic_time(us)` | 当前数据行对应AI Cube Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aic_total_cycles` | 当前数据行对应AI Cube Core执行的总周期数。 |
| `aic_cube_time(us)` | Cube 流水的活跃时间，单位为us。 |
| `aic_cube_ratio` | Cube 活跃周期占 AIC 总周期的比值。 |
| `aic_scalar_time(us)` | AIC Scalar 流水的活跃时间，单位为us。 |
| `aic_scalar_ratio` | AIC Scalar 活跃周期占总周期的比值。 |
| `aic_mte1_time(us)` | AIC MTE1 流水的活跃时间，单位为us。 |
| `aic_mte1_ratio` | AIC MTE1 活跃周期占总周期的比值。 |
| `aic_mte1_active_bw(GB/s)` | AIC MTE1 活跃期间的 L1 读取带宽，单位为GB/s。 |
| `aic_mte2_time(us)` | AIC MTE2 流水的活跃时间，单位为us。 |
| `aic_mte2_ratio` | AIC MTE2 活跃周期占总周期的比值。 |
| `aic_mte2_active_bw(GB/s)` | AIC MTE2 活跃期间的主存读取带宽，单位为GB/s。 |
| `aic_mte3_time(us)` | AIC MTE3 流水的活跃时间，单位为us。 |
| `aic_mte3_ratio` | AIC MTE3 活跃周期占总周期的比值。 |
| `aic_mte3_active_bw(GB/s)` | 代表AIC MTE3活跃期间写入L1的相关带宽，使用扣除其他通路后的L1写入计数计算，单位为GB/s。 |
| `aic_fixpipe_time(us)` | AIC Fixpipe 流水的活跃时间，单位为us。 |
| `aic_fixpipe_ratio` | AIC Fixpipe 活跃周期占总周期的比值。 |
| `aic_fixpipe_active_bw(GB/s)` | AIC Fixpipe 活跃期间的相关数据带宽，单位为GB/s。 |
| `aic_icache_miss_rate` | AIC 指令缓存未命中次数与访问总次数的比值。 |
| `aiv_time(us)` | 当前数据行对应AI Vector Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aiv_total_cycles` | 当前数据行对应AI Vector Core执行的总周期数。 |
| `aiv_vec_time(us)` | Vector 流水的活跃时间，单位为us。 |
| `aiv_vec_ratio` | Vector 活跃周期占 AIV 总周期的比值。 |
| `aiv_scalar_time(us)` | AIV Scalar 流水的活跃时间，单位为us。 |
| `aiv_scalar_ratio` | AIV Scalar 活跃周期占总周期的比值。 |
| `aiv_mte2_time(us)` | AIV MTE2 流水的活跃时间，单位为us。 |
| `aiv_mte2_ratio` | AIV MTE2 活跃周期占总周期的比值。 |
| `aiv_mte2_active_bw(GB/s)` | AIV MTE2 活跃期间的 GM 到 UB 相关带宽，单位为GB/s。 |
| `aiv_mte3_time(us)` | AIV MTE3 流水的活跃时间，单位为us。 |
| `aiv_mte3_ratio` | AIV MTE3 活跃周期占总周期的比值。 |
| `aiv_mte3_active_bw(GB/s)` | AIV MTE3活跃期间的带宽，单位为GB/s。当前所需插桩数据未提供，固定显示为NA。 |
| `aiv_icache_miss_rate` | AIV 指令缓存未命中次数与访问总次数的比值。 |

## ResourceConflictRatio（资源冲突和等待占比）

计算和搬运单元的等待周期占比、Vector资源冲突周期占比保存在ResourceConflictRatio.csv中，可用于查看各单元的等待情况以及Vector资源冲突情况。详情介绍请参见下表中的字段说明。

NA表示当前Core不适用或所需数据不可用，不表示数值零。

| 字段名 | 字段解释 |
|---|---|
| `block_id` | 当前数据行所属的逻辑Block编号。 |
| `sub_block_id` | 当前Block内的Core类型和Sub-block编号。AI Cube Core显示为cubeN，AI Vector Core显示为vectorN，其中N为Sub-block编号，不是物理核编号。 |
| `aic_time(us)` | 当前数据行对应AI Cube Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aic_total_cycles` | 当前数据行对应AI Cube Core执行的总周期数。 |
| `aic_cube_wait_ratio` | AIC Cube 等待周期占 AIC 总周期的比值。 |
| `aic_mte1_wait_ratio` | AIC MTE1 等待周期占 AIC 总周期的比值。 |
| `aic_mte2_wait_ratio` | AIC MTE2 等待周期占 AIC 总周期的比值。 |
| `aic_mte3_wait_ratio` | AIC MTE3 等待周期占 AIC 总周期的比值。 |
| `aiv_time(us)` | 当前数据行对应AI Vector Core的执行时间，根据总周期数和该Core频率换算，单位为us。 |
| `aiv_total_cycles` | 当前数据行对应AI Vector Core执行的总周期数。 |
| `aiv_vec_stu_cflt_ratio` | AIV Vector STU 冲突周期占 AIV 总周期的比值。 |
| `aiv_vec_ldu_cflt_ratio` | AIV Vector LDU 冲突周期占 AIV 总周期的比值。 |
| `aiv_vec_sfu_cflt_ratio` | AIV Vector SFU 冲突周期占 AIV 总周期的比值。冲突周期取四类SFU冲突计数之和。 |
| `aiv_vec_wait_ratio` | AIV Vector 等待周期占 AIV 总周期的比值。 |
| `aiv_mte2_wait_ratio` | AIV MTE2 等待周期占 AIV 总周期的比值。 |
| `aiv_mte3_wait_ratio` | AIV MTE3 等待周期占 AIV 总周期的比值。 |

## summary（Section汇总和算子基础信息）

所选性能指标的汇总结果和算子基础信息保存在summary.jsonl中。建议先查看其中有有效数值的算子耗时、计算活跃程度和Block执行均衡度，再查看对应CSV文件中的明细数据。详情介绍请参见下文各表中的字段说明。

`summary.jsonl` 为JSON Lines格式，每行一个JSON对象。按所选PMU Section顺序输出汇总记录，最后输出包含算子基础信息的 `OpInfoSummary`。

### Section汇总记录

| 字段名 | 字段解释 |
|---|---|
| `category` | 字符串，当前记录的Section名称：ArithmeticUtilization、L2Cache、Memory、MemoryL0、MemoryUB、PipeUtilization或ResourceConflictRatio。 |
| 对应CSV的全部指标字段 | 字段名称和单位与上述Section表相同，但去掉 `block_id` 和 `sub_block_id`。每个指标独立对有效Task级结果求算术平均，无有效值时为JSON `null`。 |

汇总始终使用Task级记录，与CSV当前选择Block级还是Task级无关。计数类指标求平均后仍不保留小数位，其余有限指标保留六位小数。不同指标可能使用不同数量的有效样本。汇总带宽是有效行带宽的平均值，不能直接当作所有Core带宽之和。

### OpInfoSummary字段

| 字段名 | JSON类型 | 字段解释 |
|---|---|---|
| `category` | string | 固定为 `OpInfoSummary`。 |
| `Op Name` | string/null | 当前Kernel的可读名称。无法获取名称时显示为null。 |
| `Op Type` | string/null | 算子执行类型：仅Cube为cube，仅Vector为vector，同时使用两者为mix。根据Task级PMU数据判断，无有效类型信息时为null。 |
| `Task Duration(us)` | number/null | 当前Kernel有效Task执行时间的中位数，单位为us。每次重放根据Task起止记录计算耗时，再取中位数；没有有效起止记录时显示为null。 |
| `Block Dim` | integer/null | Kernel启动时配置的逻辑Block数量。无法获取启动信息时显示为null。 |
| `Mix Block Dim` | null | 混合Kernel Block规模预留字段，当前固定为 `null`。 |
| `Device Id` | integer/null | Kernel启动元数据中的逻辑Device编号；不可用时为 `null`。 |
| `Pid` | integer | 执行Kernel的目标进程ID。 |
| `Current Freq` | number/null | 计算性能指标时使用的Core频率，单位为MHz。混合算子的Cube和Vector频率不一致时显示为null。 |
| `Rated Freq` | number/null | 设备接口返回的额定Core频率，单位为MHz，当前从逻辑Device 0读取。无法获取或混合算子的Cube和Vector频率不一致时为null。 |
| `aicore_parallel_utilization` | number/null | 反映各Block计算单元的平均活跃程度。先分别计算同一Block内Cube和Vector的平均活跃占比，再相加并对各Block取平均。未采集PipeUtilization或无有效数据时为null；结果可能大于1。 |
| `aicore_parallel_balance` | number/null | 反映各Block执行时间的一致程度。每个Block取其中最长的Core执行时间，根据各Block耗时的离散程度计算，值越接近1表示越均衡。无有效数据时为null，耗时差异较大时可能为负数。 |
| `aicore_gm_bw_theoretical(GB/s)` | integer/null | 选择Memory时固定为1600 GB/s，用于计算汇总GM带宽利用率；否则为 `null`。 |
| `aicore_gm_read_bw(GB/s)` | number/null | Memory汇总中的AIC与AIV主存读取带宽相加；仅一类有效时取该值，两类均无效时为 `null`。 |
| `aicore_gm_write_bw(GB/s)` | number/null | Memory汇总中的AIC与AIV主存写入带宽相加；仅一类有效时取该值，两类均无效时为 `null`。 |
| `aicore_gm_bw_usage_rate(%)` | number/null | GM读写汇总带宽之和占固定理论带宽1600 GB/s的百分比。缺少读或写汇总带宽时为null，结果可能超过100%。 |

## HardwareInfo（主机和设备信息）

采集时的主机和设备信息保存在HardwareInfo.jsonl中，包括设备型号、Core数量、频率及内存容量等。详情介绍请参见下文各表中的字段说明。

`HardwareInfo.jsonl` 为采集环境快照，每行一个JSON对象。固定包含以下五个category，顺序为Host Info、Device Info、CPU Information、AI Core Information、Memory Information。

HBM总容量来自平台接口，NPU数量为Runtime可见设备数。该文件并不是对每个Kernel实际Device分别生成的快照，跨设备比较时应结合summary中的 `Device Id`。查询失败的字段可能保留0或空字符串，不能一律解释为真实零值。容量最多保留两位小数并去掉末尾零，频率和计数为整数；

### Host Info

| 字段名 | JSON类型 | 字段解释 |
|---|---|---|
| `category` | string | 固定为 `Host Info`。 |
| `cpu physical count` | integer | 主机在线CPU所属的物理处理器数量，即Socket/Package数量，不是物理核数量。 |
| `cpu logical count` | integer | 主机配置的逻辑CPU数量，不一定等于当前在线CPU数量或程序线程数。 |
| `memory total size(MB)` | number | 主机物理内存总容量，单位为MB，不是空闲内存或目标程序占用量。 |
| `disk total size(GB)` | number | 工具查询路径所在文件系统的总容量，单位为GB，不是剩余空间。 |

### Device Info

| 字段名 | JSON类型 | 字段解释 |
|---|---|---|
| `category` | string | 固定为 `Device Info`。 |
| `npu count` | integer | Runtime可见NPU设备数量，不表示本次Kernel使用数量。 |
| `chip info` | string | SoC名称与非空芯片版本用空格连接。 |
| `arch info` | string | 设备的NPU架构编号，以字符串表示，例如3510。 |

### CPU Information

| 字段名 | JSON类型 | 字段解释 |
|---|---|---|
| `category` | string | 固定为 `CPU Information`。 |
| `control cpu count` | integer | 设备侧Control CPU核数。 |
| `ai cpu count` | integer | 设备侧AI CPU核数。 |
| `ai cpu frequency(MHZ)` | integer | 设备AI CPU当前频率，单位为MHz。 |

### AI Core Information

| 字段名 | JSON类型 | 字段解释 |
|---|---|---|
| `category` | string | 固定为 `AI Core Information`。 |
| `ai core count` | integer | 设备接口报告的AI Core数量，按接口原值记录。 |
| `ai cube count` | integer | 设备Cube Core数量。 |
| `ai vector count` | integer | 设备Vector Core数量。 |
| `ai cube frequency(MHZ)` | integer | Cube Core频率，单位为MHz；优先当前频率，查询失败回退固定频率，再失败使用1650。 |
| `ai vector frequency(MHZ)` | integer | Vector Core频率，单位为MHz；优先当前频率，查询失败回退固定频率，再失败使用1650。 |

### Memory Information

| 字段名 | JSON类型 | 字段解释 |
|---|---|---|
| `category` | string | 固定为 `Memory Information`。 |
| `hbm total(MB)` | number | 平台接口报告的HBM总容量，单位为MB。 |
| `hbm used(MB)` | number | Runtime可分配HBM总量减去空闲量后得到的已使用容量，单位为MB。 |
| `hbm frequency(MHZ)` | integer | 设备接口返回的HBM频率，单位为MHz。 |

## Pipeline（流水时间线）

采样流水的忙碌区间保存在PipeTrace.json中，可用于查看同一次重放中各采样流水的忙碌时间及区间重叠情况。详情介绍请参见下文各表中的字段说明。

`PipeTrace.json` 采用Trace Event格式，每个事件记录一条流水从忙碌开始到忙碌结束的完整区间。事件开始时间以当前重放中最早的有效采样时间为起点。

### 顶层字段

| 字段名 | JSON类型 | 字段解释 |
|---|---|---|
| `displayTimeUnit` | string | 固定为 `ns`，是查看器展示单位；不改变 `ts` 和 `dur` 的us数据单位。 |
| `profilingType` | string | 固定为 `op`。 |
| `schemaVersion` | integer | 当前结构版本为1。 |
| `traceEvents` | array | 完整流水区间事件数组，允许为空。 |

### 事件字段

| 字段名 | JSON类型 | 字段解释 |
|---|---|---|
| `cname` | string | 流水轨道的颜色名称，见下表。 |
| `dur` | number | 忙碌区间持续时间，单位为us，有限且非负。 |
| `name` | string | 流水名称：SCALAR、VECTOR、CUBE、MTE1、MTE2、MTE3或FIXP。 |
| `ph` | string | 固定为 `X`，表示完整区间事件。 |
| `pid` | string | 来源和Core轨道标识，格式见下文；此字段不是操作系统进程ID。 |
| `tid` | string | 流水轨道标识，与 `name` 相同。 |
| `ts` | number | 相对于该Replay时间原点的区间开始时间，单位为us，有限且非负。 |

### 流水和轨道

| 流水名称 | `cname` | 含义 |
|---|---|---|
| SCALAR | startup | 标量流水忙碌区间。 |
| VECTOR | rail_idle | Vector流水忙碌区间。 |
| CUBE | rail_response | Cube流水忙碌区间。 |
| MTE1 | thread_state_iowait | MTE1搬运流水忙碌区间。 |
| MTE2 | yellow | MTE2搬运流水忙碌区间。 |
| MTE3 | rail_animation | MTE3搬运流水忙碌区间。 |
| FIXP | thread_state_unknown | Fixpipe流水忙碌区间。 |

单个来源片段的 `pid` 为 `group<group_id>.<core>`；多个来源片段合并时为：

```text
process<process_ordinal>.result<result_sequence>.device<device_id>.replay<replay_id>.group<group_id>.<core>
```

其中process ordinal是来源进程目录的排序序号，result sequence是结果序号，device id和replay id用于区分采集来源。`core` 为 `cubecore`、`veccore0` 或 `veccore1`。轨道由 `(pid, tid)` 共同确定。

`group_id` 范围为0～5，最多6个采样Group；它不等于逻辑Block或物理Core编号。即使Kernel启用了全部Core，时间线也最多展示6个Group，不能根据轨道数推断实际启用核数。Block标识未写入事件。

事件结束时间为开始时间加持续时间，数组顺序不代表时间顺序。不同轨道可以重叠，多Replay之间也没有由此建立统一连续时间轴。只写入起止齐全的忙碌区间，缺少端点的区间会跳过；空数组仅表示没有完整区间。不同流水的忙碌时间可能重叠，累计忙碌时间不能直接等同于Kernel执行时长。
