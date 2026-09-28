# npu-compute performance data

## ArithmeticUtilization (Cube and Vector instruction utilization and counts)

ArithmeticUtilization.csv contains active-cycle ratios for Cube and Vector instructions and instruction counts for Cube. Use it to examine activity by instruction type and the number of Cube instructions executed. See the field descriptions in the table below for details.

NA means inapplicable to the current Core or unavailable data; it does not mean zero.

| Field | Description |
|---|---|
| `block_id` | Logical Block identifier for this row. |
| `sub_block_id` | Core type and Sub-block identifier: cubeN for AI Cube Core or vectorN for AI Vector Core. N is the Sub-block number, not the physical Core number. |
| `aic_time(us)` | Execution time on the AI Cube Core for this row, converted from total cycles using its frequency, in us. |
| `aic_total_cycles` | Total execution cycles on the AI Cube Core for this row. |
| `aic_cube_ratio` | Cube active cycles as a fraction of total AI Cube Core cycles. |
| `aic_cube_fp_ratio` | Cube floating-point instructions active cycles as a fraction of total AI Cube Core cycles. |
| `aic_cube_int_ratio` | Cube integer instructions active cycles as a fraction of total AI Cube Core cycles. |
| `aic_cube_total_instr_number` | Total number of instructions executed by Cube. |
| `aic_cube_fp_instr_number` | Number of floating-point instructions executed by Cube. |
| `aic_cube_int_instr_number` | Number of integer instructions executed by Cube. |
| `aiv_time(us)` | Execution time on the AI Vector Core for this row, converted from total cycles using its frequency, in us. |
| `aiv_total_cycles` | Total execution cycles on the AI Vector Core for this row. |
| `aiv_vec_ratio` | Vector active cycles as a fraction of total AI Vector Core cycles. |
| `aiv_vec_vf_ratio` | Vector VF instructions active cycles as a fraction of total AI Vector Core cycles. |
| `aiv_vec_sfu_ratio` | Vector SFU instructions active cycles as a fraction of total AI Vector Core cycles. |
| `aiv_vec_simt_vf_ratio` | SIMT VF instructions active cycles as a fraction of total AI Vector Core cycles. |

## L2Cache (L2 cache access counts and hit rates)

L2Cache.csv contains L2 read/write counts and hit rates. Use it to examine hits, misses, and evictions for near and far L2 Cache accesses by AI Cube Core and AI Vector Core. See the field descriptions in the table below for details.

NA means inapplicable to the current Core or unavailable data; it does not mean zero.

| Field | Description |
|---|---|
| `block_id` | Logical Block identifier for this row. |
| `sub_block_id` | Core type and Sub-block identifier: cubeN for AI Cube Core or vectorN for AI Vector Core. N is the Sub-block number, not the physical Core number. |
| `aic_time(us)` | Execution time on the AI Cube Core for this row, converted from total cycles using its frequency, in us. |
| `aic_total_cycles` | Total execution cycles on the AI Cube Core for this row. |
| `aic_read_close_hit` | Number of near L2 read hits for the AI Cube Core. |
| `aic_read_close_miss` | Number of near L2 read misses for the AI Cube Core. |
| `aic_read_close_victim` | Number of near L2 read evictions for the AI Cube Core. |
| `aic_read_far_hit` | Number of far L2 read hits for the AI Cube Core. |
| `aic_read_far_miss` | Number of far L2 read misses for the AI Cube Core. |
| `aic_read_far_victim` | Number of far L2 read evictions for the AI Cube Core. |
| `aic_read_hit_rate(%)` | L2 read hit rate for the AI Cube Core, in %. The hit count includes near and far hits; the total includes near and far hits, misses, and evictions. A complete set of counters with zero total produces 0. |
| `aic_write_close_hit` | Number of near L2 write hits for the AI Cube Core. |
| `aic_write_close_miss` | Number of near L2 write misses for the AI Cube Core. |
| `aic_write_close_victim` | Number of near L2 write evictions for the AI Cube Core. |
| `aic_write_far_hit` | Number of far L2 write hits for the AI Cube Core. |
| `aic_write_far_miss` | Number of far L2 write misses for the AI Cube Core. |
| `aic_write_far_victim` | Number of far L2 write evictions for the AI Cube Core. |
| `aic_write_hit_rate(%)` | L2 write hit rate for the AI Cube Core, in %. The hit count includes near and far hits; the total includes near and far hits, misses, and evictions. A complete set of counters with zero total produces 0. |
| `aiv_time(us)` | Execution time on the AI Vector Core for this row, converted from total cycles using its frequency, in us. |
| `aiv_total_cycles` | Total execution cycles on the AI Vector Core for this row. |
| `aiv_read_close_hit` | Number of near L2 read hits for the AI Vector Core. |
| `aiv_read_close_miss` | Number of near L2 read misses for the AI Vector Core. |
| `aiv_read_close_victim` | Number of near L2 read evictions for the AI Vector Core. |
| `aiv_read_far_hit` | Number of far L2 read hits for the AI Vector Core. |
| `aiv_read_far_miss` | Number of far L2 read misses for the AI Vector Core. |
| `aiv_read_far_victim` | Number of far L2 read evictions for the AI Vector Core. |
| `aiv_read_hit_rate(%)` | L2 read hit rate for the AI Vector Core, in %. The hit count includes near and far hits; the total includes near and far hits, misses, and evictions. A complete set of counters with zero total produces 0. |
| `aiv_write_close_hit` | Number of near L2 write hits for the AI Vector Core. |
| `aiv_write_close_miss` | Number of near L2 write misses for the AI Vector Core. |
| `aiv_write_close_victim` | Number of near L2 write evictions for the AI Vector Core. |
| `aiv_write_far_hit` | Number of far L2 write hits for the AI Vector Core. |
| `aiv_write_far_miss` | Number of far L2 write misses for the AI Vector Core. |
| `aiv_write_far_victim` | Number of far L2 write evictions for the AI Vector Core. |
| `aiv_write_hit_rate(%)` | L2 write hit rate for the AI Vector Core, in %. The hit count includes near and far hits; the total includes near and far hits, misses, and evictions. A complete set of counters with zero total produces 0. |

## Memory (Memory bandwidth and transfer volume)

Memory.csv contains main-memory, L1, and UB bandwidth, transfer volumes, and transfer instruction information. Use it to examine main-memory reads and writes and transfers along the data paths. See the field descriptions in the table below for details.

Bandwidth is calculated from transferred data and execution time, in GB/s. Mixed Kernels use a valid Task duration when available; otherwise the current Core execution time is used.

NA means inapplicable to the current Core or unavailable data; it does not mean zero.

| Field | Description |
|---|---|
| `block_id` | Logical Block identifier for this row. |
| `sub_block_id` | Core type and Sub-block identifier: cubeN for AI Cube Core or vectorN for AI Vector Core. N is the Sub-block number, not the physical Core number. |
| `aic_time(us)` | Execution time on the AI Cube Core for this row, converted from total cycles using its frequency, in us. |
| `aic_total_cycles` | Total execution cycles on the AI Cube Core for this row. |
| `aic_l1_read_bw(GB/s)` | Average bandwidth for reading data from L1, in GB/s. |
| `aic_l1_write_bw(GB/s)` | Average bandwidth for writing data into L1, in GB/s. |
| `aic_main_mem_read_bw(GB/s)` | Average main-memory read bandwidth of the AI Cube Core, in GB/s. |
| `aic_main_mem_write_bw(GB/s)` | Average main-memory write bandwidth of the AI Cube Core, in GB/s. |
| `aic_mte1_instructions` | Number of MTE1 instructions executed by the AI Cube Core. |
| `aic_mte1_ratio` | MTE1 active cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte2_instructions` | Number of MTE2 instructions executed by the AI Cube Core. |
| `aic_mte2_ratio` | MTE2 active cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte3_instructions` | Number of MTE3 instructions executed by the AI Cube Core. |
| `aic_mte3_ratio` | MTE3 active cycles as a fraction of total AI Cube Core cycles. |
| `aiv_time(us)` | Execution time on the AI Vector Core for this row, converted from total cycles using its frequency, in us. |
| `aiv_total_cycles` | Total execution cycles on the AI Vector Core for this row. |
| `aiv_ub_to_gm_bw(GB/s)` | Bandwidth for transferring data from UB to GM, in GB/s. Currently always NA because the required instrumentation data is unavailable. |
| `aiv_gm_to_ub_bw(GB/s)` | Average bandwidth for transferring data from GM to UB, in GB/s. |
| `aiv_main_mem_read_bw(GB/s)` | Average main-memory read bandwidth of the AI Vector Core, in GB/s. |
| `aiv_main_mem_write_bw(GB/s)` | Average main-memory write bandwidth of the AI Vector Core, in GB/s. |
| `aiv_mte2_instructions` | Number of MTE2 instructions executed by the AI Vector Core. |
| `aiv_mte2_ratio` | MTE2 active cycles as a fraction of total AI Vector Core cycles. |
| `aiv_mte3_instructions` | Number of MTE3 instructions executed by the AI Vector Core. |
| `aiv_mte3_ratio` | MTE3 active cycles as a fraction of total AI Vector Core cycles. |
| `read_main_memory_datas(KB)` | Amount of main-memory data read by the Core represented by this row, in KB. |
| `write_main_memory_datas(KB)` | Amount of main-memory data written by the Core represented by this row, in KB. |
| `GM_to_L1_datas(KB)` | Amount of data transferred from GM to L1, in KB, for the current Core row. |
| `GM_to_L1_bw_usage_rate(%)` | Average bandwidth from GM to L1 as a percentage of the configured maximum for that path, capped at 100%. |
| `L0C_to_L1_datas(KB)` | Amount of data transferred from L0C to L1, in KB, for the current Core row. |
| `L0C_to_L1_bw_usage_rate(%)` | Average bandwidth from L0C to L1 as a percentage of the configured maximum for that path, capped at 100%. |
| `L0C_to_GM_datas(KB)` | Amount of data transferred from L0C to GM, in KB, for the current Core row. |
| `L0C_to_GM_bw_usage_rate(%)` | Average bandwidth from L0C to GM as a percentage of the configured maximum for that path, capped at 100%. |
| `GM_to_UB_datas(KB)` | Amount of data transferred from GM to UB, in KB, for the current Core row. |
| `GM_to_UB_bw_usage_rate(%)` | Average bandwidth from GM to UB as a percentage of the configured maximum for that path, capped at 100%. |
| `UB_to_GM_datas(KB)` | Amount of data transferred from UB to GM, in KB. Currently always NA because the required instrumentation data is unavailable. |
| `UB_to_GM_bw_usage_rate(%)` | Bandwidth utilization of the UB-to-GM path, expressed as a percentage. Currently always NA because the required instrumentation data is unavailable. |

GM-to-UB volume uses the main-memory read count after subtracting two DCache-miss-related read counts, with negative differences treated as zero. GM-to-L1 and L0C-to-GM use the AIC main-memory read and write counts, respectively.

## MemoryL0 (L0 read and write bandwidth)

MemoryL0.csv contains bandwidth between L0A, L0B, L0C and Cube or L1. Use it to examine the average bandwidth of each data path. See the field descriptions in the table below for details.

Bandwidth fields apply to AI Cube Core. Bandwidth is calculated from transferred data and execution time, in GB/s. Mixed Kernels use a valid Task duration when available; otherwise the current Core execution time is used.

NA means inapplicable to the current Core or unavailable data; it does not mean zero.

| Field | Description |
|---|---|
| `block_id` | Logical Block identifier for this row. |
| `sub_block_id` | Core type and Sub-block identifier: cubeN for AI Cube Core or vectorN for AI Vector Core. N is the Sub-block number, not the physical Core number. |
| `aic_time(us)` | Execution time on the AI Cube Core for this row, converted from total cycles using its frequency, in us. |
| `aic_total_cycles` | Total execution cycles on the AI Cube Core for this row. |
| `aic_l0a_read_bw(GB/s)` | Average bandwidth for transferring data from L0A to Cube, in GB/s. |
| `aic_l0a_write_bw(GB/s)` | Average bandwidth for transferring data from L1 to L0A, in GB/s. |
| `aic_l0b_read_bw(GB/s)` | Average bandwidth for transferring data from L0B to Cube, in GB/s. |
| `aic_l0b_write_bw(GB/s)` | Average bandwidth for transferring data from L1 to L0B, in GB/s. |
| `aic_l0c_read_bw_cube(GB/s)` | Average bandwidth for Cube reading data from L0C, in GB/s. |
| `aic_l0c_write_bw_cube(GB/s)` | Average bandwidth for Cube writing data into L0C, in GB/s. |
| `aiv_time(us)` | Execution time on the AI Vector Core for this row, converted from total cycles using its frequency, in us. |
| `aiv_total_cycles` | Total execution cycles on the AI Vector Core for this row. |

## MemoryUB (UB read and write bandwidth)

MemoryUB.csv contains bandwidth between UB and Vector or GM. Use it to examine the average bandwidth of Vector accesses to UB and transfers from GM to UB. See the field descriptions in the table below for details.

Bandwidth fields apply to AI Vector Core. Bandwidth is calculated from transferred data and execution time, in GB/s. Mixed Kernels use a valid Task duration when available; otherwise the current Core execution time is used.

NA means inapplicable to the current Core or unavailable data; it does not mean zero.

| Field | Description |
|---|---|
| `block_id` | Logical Block identifier for this row. |
| `sub_block_id` | Core type and Sub-block identifier: cubeN for AI Cube Core or vectorN for AI Vector Core. N is the Sub-block number, not the physical Core number. |
| `aic_time(us)` | Execution time on the AI Cube Core for this row, converted from total cycles using its frequency, in us. |
| `aic_total_cycles` | Total execution cycles on the AI Cube Core for this row. |
| `aiv_time(us)` | Execution time on the AI Vector Core for this row, converted from total cycles using its frequency, in us. |
| `aiv_total_cycles` | Total execution cycles on the AI Vector Core for this row. |
| `aiv_ub_read_bw_vector(GB/s)` | Average bandwidth for Vector reading data from UB, in GB/s. |
| `aiv_ub_write_bw_vector(GB/s)` | Average bandwidth for Vector writing data into UB, in GB/s. |
| `aiv_ub_read_bw_gm(GB/s)` | Bandwidth for reading UB data for transfer to GM, in GB/s. Currently always NA because the required instrumentation data is unavailable. |
| `aiv_ub_write_bw_gm(GB/s)` | Average bandwidth for transferring data from GM into UB, in GB/s. |

## PipeUtilization (Compute and transfer pipeline utilization)

PipeUtilization.csv contains compute and transfer pipeline active times, cycle ratios, active bandwidth, and instruction-cache miss rates. Use it to examine pipeline active times and cycle ratios. See the field descriptions in the table below for details.

Active bandwidth uses the corresponding pipeline active time. Some bandwidth fields also require data from Memory or MemoryUB and may be NA when this Section is collected alone.

NA means inapplicable to the current Core or unavailable data; it does not mean zero.

| Field | Description |
|---|---|
| `block_id` | Logical Block identifier for this row. |
| `sub_block_id` | Core type and Sub-block identifier: cubeN for AI Cube Core or vectorN for AI Vector Core. N is the Sub-block number, not the physical Core number. |
| `aic_time(us)` | Execution time on the AI Cube Core for this row, converted from total cycles using its frequency, in us. |
| `aic_total_cycles` | Total execution cycles on the AI Cube Core for this row. |
| `aic_cube_time(us)` | Active time of the Cube pipeline on the AI Cube Core, in us. |
| `aic_cube_ratio` | Cube active cycles as a fraction of total AI Cube Core cycles. |
| `aic_scalar_time(us)` | Active time of the Scalar pipeline on the AI Cube Core, in us. |
| `aic_scalar_ratio` | Scalar active cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte1_time(us)` | Active time of the MTE1 pipeline on the AI Cube Core, in us. |
| `aic_mte1_ratio` | MTE1 active cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte1_active_bw(GB/s)` | Bandwidth for reading L1 data during MTE1 activity, in GB/s. |
| `aic_mte2_time(us)` | Active time of the MTE2 pipeline on the AI Cube Core, in us. |
| `aic_mte2_ratio` | MTE2 active cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte2_active_bw(GB/s)` | Bandwidth for reading main-memory data during MTE2 activity, in GB/s. |
| `aic_mte3_time(us)` | Active time of the MTE3 pipeline on the AI Cube Core, in us. |
| `aic_mte3_ratio` | MTE3 active cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte3_active_bw(GB/s)` | Bandwidth for writing L1 data during MTE3 activity, using the adjusted L1 write count, in GB/s. |
| `aic_fixpipe_time(us)` | Active time of the Fixpipe pipeline on the AI Cube Core, in us. |
| `aic_fixpipe_ratio` | Fixpipe active cycles as a fraction of total AI Cube Core cycles. |
| `aic_fixpipe_active_bw(GB/s)` | Bandwidth for Fixpipe transfers during Fixpipe activity, in GB/s. |
| `aic_icache_miss_rate` | Number of instruction-cache misses divided by the total number of accesses for the AI Cube Core. |
| `aiv_time(us)` | Execution time on the AI Vector Core for this row, converted from total cycles using its frequency, in us. |
| `aiv_total_cycles` | Total execution cycles on the AI Vector Core for this row. |
| `aiv_vec_time(us)` | Active time of the Vector pipeline on the AI Vector Core, in us. |
| `aiv_vec_ratio` | Vector active cycles as a fraction of total AI Vector Core cycles. |
| `aiv_scalar_time(us)` | Active time of the Scalar pipeline on the AI Vector Core, in us. |
| `aiv_scalar_ratio` | Scalar active cycles as a fraction of total AI Vector Core cycles. |
| `aiv_mte2_time(us)` | Active time of the MTE2 pipeline on the AI Vector Core, in us. |
| `aiv_mte2_ratio` | MTE2 active cycles as a fraction of total AI Vector Core cycles. |
| `aiv_mte2_active_bw(GB/s)` | Bandwidth for GM-to-UB transfers during MTE2 activity, in GB/s. |
| `aiv_mte3_time(us)` | Active time of the MTE3 pipeline on the AI Vector Core, in us. |
| `aiv_mte3_ratio` | MTE3 active cycles as a fraction of total AI Vector Core cycles. |
| `aiv_mte3_active_bw(GB/s)` | Bandwidth during AI Vector Core MTE3 activity, in GB/s. Currently always NA because the required instrumentation data is unavailable. |
| `aiv_icache_miss_rate` | Number of instruction-cache misses divided by the total number of accesses for the AI Vector Core. |

## ResourceConflictRatio (Resource conflict and wait ratios)

ResourceConflictRatio.csv contains compute and transfer wait-cycle ratios and Vector resource-conflict ratios. Use it to examine waits in each unit and Vector resource conflicts. See the field descriptions in the table below for details.

NA means inapplicable to the current Core or unavailable data; it does not mean zero.

| Field | Description |
|---|---|
| `block_id` | Logical Block identifier for this row. |
| `sub_block_id` | Core type and Sub-block identifier: cubeN for AI Cube Core or vectorN for AI Vector Core. N is the Sub-block number, not the physical Core number. |
| `aic_time(us)` | Execution time on the AI Cube Core for this row, converted from total cycles using its frequency, in us. |
| `aic_total_cycles` | Total execution cycles on the AI Cube Core for this row. |
| `aic_cube_wait_ratio` | Cube wait cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte1_wait_ratio` | MTE1 wait cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte2_wait_ratio` | MTE2 wait cycles as a fraction of total AI Cube Core cycles. |
| `aic_mte3_wait_ratio` | MTE3 wait cycles as a fraction of total AI Cube Core cycles. |
| `aiv_time(us)` | Execution time on the AI Vector Core for this row, converted from total cycles using its frequency, in us. |
| `aiv_total_cycles` | Total execution cycles on the AI Vector Core for this row. |
| `aiv_vec_stu_cflt_ratio` | Vector STU conflict cycles as a fraction of total AI Vector Core cycles. |
| `aiv_vec_ldu_cflt_ratio` | Vector LDU conflict cycles as a fraction of total AI Vector Core cycles. |
| `aiv_vec_sfu_cflt_ratio` | Vector SFU conflict cycles as a fraction of total AI Vector Core cycles. Includes the sum of four SFU conflict counters. |
| `aiv_vec_wait_ratio` | Vector wait cycles as a fraction of total AI Vector Core cycles. |
| `aiv_mte2_wait_ratio` | MTE2 wait cycles as a fraction of total AI Vector Core cycles. |
| `aiv_mte3_wait_ratio` | MTE3 wait cycles as a fraction of total AI Vector Core cycles. |

## summary (Section summaries and operator information)

summary.jsonl contains summaries of the selected performance metrics and basic operator information. Start with valid values for operator duration, compute activity, and Block execution balance, then examine the detailed data in the corresponding CSV files. See the field descriptions in the following tables for details.

`summary.jsonl` uses JSON Lines: one JSON object per line. Selected PMU Section records appear in selection order, followed by `OpInfoSummary`, which contains basic operator information.

### Section summary records

| Field | Description |
|---|---|
| `category` | String identifying the Section: ArithmeticUtilization, L2Cache, Memory, MemoryL0, MemoryUB, PipeUtilization, or ResourceConflictRatio. |
| All metric fields from the corresponding CSV | Same names and units as the Section tables above, excluding `block_id` and `sub_block_id`. Each metric is independently averaged over valid Task-level values; no valid value produces JSON `null`. |

Summaries always use Task records regardless of whether CSV uses Block or Task records. Counter averages are still displayed without decimal places; other finite metrics use six. Metrics may have different valid sample counts. Summary bandwidth is a mean of valid row bandwidths, not the sum across all Cores.

### OpInfoSummary fields

| Field | JSON type | Description |
|---|---|---|
| `category` | string | Always `OpInfoSummary`. |
| `Op Name` | string/null | Readable name of the current Kernel. Null if the name is unavailable. |
| `Op Type` | string/null | Execution type derived from Task PMU data: cube for Cube only, vector for Vector only, or mix for both. Null if no valid type information exists. |
| `Task Duration(us)` | number/null | Median valid Task execution time for the current Kernel, in us. Each replay duration comes from Task start and end records. Null when no valid interval is available. |
| `Block Dim` | integer/null | Logical Block count configured at Kernel launch. Null if launch information is unavailable. |
| `Mix Block Dim` | null | Reserved mixed-Kernel Block dimension field; currently always `null`. |
| `Device Id` | integer/null | Logical Device ID from Kernel launch metadata; `null` if unavailable. |
| `Pid` | integer | ID of the target process executing the Kernel. |
| `Current Freq` | number/null | Core frequency used to calculate performance metrics, in MHz. Null when Cube and Vector frequencies differ for a mixed Kernel. |
| `Rated Freq` | number/null | Rated Core frequency returned by the device API, in MHz, currently queried for logical Device 0. Null if unavailable or if Cube and Vector frequencies differ for a mixed Kernel. |
| `aicore_parallel_utilization` | number/null | Average compute activity across Blocks. Average Cube and Vector active ratios separately within each Block, add them, then average across Blocks. Null without PipeUtilization or valid data; may exceed 1. |
| `aicore_parallel_balance` | number/null | Consistency of execution times across Blocks, using the longest Core duration in each Block. Values closer to 1 indicate more balanced execution. Null without valid data; large variation can produce a negative value. |
| `aicore_gm_bw_theoretical(GB/s)` | integer/null | Theoretical bandwidth used to calculate summary GM bandwidth utilization. Fixed at 1600 GB/s when Memory is selected, otherwise `null`. |
| `aicore_gm_read_bw(GB/s)` | number/null | Sum of AIC and AIV main-memory read bandwidths in the Memory summary. Uses the available one if only one exists; `null` if neither exists. |
| `aicore_gm_write_bw(GB/s)` | number/null | Sum of AIC and AIV main-memory write bandwidths in the Memory summary. Uses the available one if only one exists; `null` if neither exists. |
| `aicore_gm_bw_usage_rate(%)` | number/null | Sum of summary GM read and write bandwidth as a percentage of the fixed theoretical bandwidth of 1600 GB/s. Null if either summary is unavailable; may exceed 100%. |

## HardwareInfo (host and device information)

HardwareInfo.jsonl contains host and device information recorded during collection, including the device model, Core counts, frequencies, and memory capacities. See the field descriptions in the following tables for details.

`HardwareInfo.jsonl` is an environment snapshot with one JSON object per line. It contains five categories in this order: Host Info, Device Info, CPU Information, AI Core Information, Memory Information.

Total HBM capacity comes from the platform API; NPU count is the Runtime-visible device count. This is not a separate snapshot for every Kernel Device. Check `Device Id` in the summary when comparing devices. Failed queries may leave 0 or an empty string; these are not necessarily real zero values. Capacities have at most two decimal places with trailing zeros removed; frequencies and counts are integers.

### Host Info

| Field | JSON type | Description |
|---|---|---|
| `category` | string | Always `Host Info`. |
| `cpu physical count` | integer | Number of physical processor packages associated with online host CPUs; not physical Core count. |
| `cpu logical count` | integer | Configured host logical CPU count; may differ from online CPUs or application threads. |
| `memory total size(MB)` | number | Total host physical memory, in MB; not free memory or target-process usage. |
| `disk total size(GB)` | number | Total capacity of the filesystem at the queried path, in GB; not free space. |

### Device Info

| Field | JSON type | Description |
|---|---|---|
| `category` | string | Always `Device Info`. |
| `npu count` | integer | Runtime-visible NPU count, not the number used by this Kernel. |
| `chip info` | string | SoC name followed by a non-empty chip version, separated by a space. |
| `arch info` | string | NPU architecture identifier as a string, for example 3510. |

### CPU Information

| Field | JSON type | Description |
|---|---|---|
| `category` | string | Always `CPU Information`. |
| `control cpu count` | integer | Number of device-side Control CPU Cores. |
| `ai cpu count` | integer | Number of device-side AI CPU Cores. |
| `ai cpu frequency(MHZ)` | integer | Current device AI CPU frequency, in MHz. |

### AI Core Information

| Field | JSON type | Description |
|---|---|---|
| `category` | string | Always `AI Core Information`. |
| `ai core count` | integer | AI Core count as reported by the device API. |
| `ai cube count` | integer | Number of device Cube Cores. |
| `ai vector count` | integer | Number of device Vector Cores. |
| `ai cube frequency(MHZ)` | integer | Cube Core frequency in MHz: current frequency first, fixed frequency on query failure, then 1650 as the final fallback. |
| `ai vector frequency(MHZ)` | integer | Vector Core frequency in MHz: current frequency first, fixed frequency on query failure, then 1650 as the final fallback. |

### Memory Information

| Field | JSON type | Description |
|---|---|---|
| `category` | string | Always `Memory Information`. |
| `hbm total(MB)` | number | Total HBM capacity reported by the platform API, in MB. |
| `hbm used(MB)` | number | Used HBM capacity obtained by subtracting free memory from Runtime-allocatable memory, in MB. |
| `hbm frequency(MHZ)` | integer | HBM frequency returned by the device API, in MHz. |

## Pipeline (pipeline timeline)

PipeTrace.json contains busy intervals of sampled pipelines. Use it to examine busy times and overlapping intervals of sampled pipelines within the same replay. See the field descriptions in the following tables for details.

`PipeTrace.json` uses the Trace Event format. Each event represents a complete interval from the start to the end of pipeline activity. Event start times are relative to the earliest valid sample timestamp in the current replay.

### Top-level fields

| Field | JSON type | Description |
|---|---|---|
| `displayTimeUnit` | string | Always `ns`, the viewer display unit. It does not change the us unit of `ts` or `dur`. |
| `profilingType` | string | Always `op`. |
| `schemaVersion` | integer | Current schema version: 1. |
| `traceEvents` | array | Array of complete pipeline interval events; may be empty. |

### Event fields

| Field | JSON type | Description |
|---|---|---|
| `cname` | string | Color name for the pipeline track, listed below. |
| `dur` | number | Busy-interval duration in us, finite and non-negative. |
| `name` | string | Pipeline name: SCALAR, VECTOR, CUBE, MTE1, MTE2, MTE3, or FIXP. |
| `ph` | string | Always `X`, denoting a complete interval event. |
| `pid` | string | Source and Core track identifier, as described below; not an operating-system process ID. |
| `tid` | string | Pipeline track identifier; identical to `name`. |
| `ts` | number | Interval start relative to the Replay time origin, in us, finite and non-negative. |

### Pipelines and tracks

| Pipeline | `cname` | Meaning |
|---|---|---|
| SCALAR | startup | Scalar pipeline busy interval. |
| VECTOR | rail_idle | Vector pipeline busy interval. |
| CUBE | rail_response | Cube pipeline busy interval. |
| MTE1 | thread_state_iowait | MTE1 transfer pipeline busy interval. |
| MTE2 | yellow | MTE2 transfer pipeline busy interval. |
| MTE3 | rail_animation | MTE3 transfer pipeline busy interval. |
| FIXP | thread_state_unknown | Fixpipe pipeline busy interval. |

For a single source fragment, `pid` is `group<group_id>.<core>`. When multiple source fragments are merged, it becomes:

```text
process<process_ordinal>.result<result_sequence>.device<device_id>.replay<replay_id>.group<group_id>.<core>
```

The process ordinal is the sorted source-directory index; result sequence, device ID, and replay ID distinguish collection sources. `core` is `cubecore`, `veccore0`, or `veccore1`. A track is identified by `(pid, tid)`.

`group_id` ranges from 0 to 5, for at most six sampled Groups. It is not a logical Block or physical Core ID. Even when the Kernel uses all Cores, at most six Groups are shown; track count cannot determine how many Cores the Kernel used. Block identifiers are not exported in events.

An event ends at its start time plus its duration; array order is not time order. Tracks can overlap, and merging replays does not create a shared continuous time axis. Only busy intervals with both endpoints are written; incomplete intervals are skipped. An empty array means there are no complete intervals. Summing `dur` does not directly give Kernel execution time.
