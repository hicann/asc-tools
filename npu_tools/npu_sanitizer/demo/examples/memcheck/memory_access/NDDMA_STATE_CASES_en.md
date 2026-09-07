# NDDMA State Updates and Legal Boundaries

## API References

These cases follow the devkit document `DataCopy_GMToUB_NDDMA.md` and its linked example:
`examples/01_simd_cpp_api/03_basic_api/00_data_movement/data_copy_gm2ub_nddma/multidimensional_data_movement.asc`.
They use the Basic APIs `NdDmaLoopInfo`, `NdDmaParams`, `NdDmaDci`, and `DataCopy` on dav-3510.
The NDDMA cache is refreshed before each transfer, and `PipeBarrier<PIPE_ALL>` runs before UB reuse.

## Cases

All cases use `nddma_scenarios_memory_case.asc`. Each name has both `_valid` and `_oob` targets.

| Name Prefix | Profile | Valid Allocation Bytes | OOB Allocation Bytes | Check |
| --- | --- | --- | --- | --- |
| nddma_scenarios_state_axis0 | 6 | 343 | 342 | loop0 stride 1 -> 2 -> 1 |
| nddma_scenarios_state_axis1 | 7 | 346 | 345 | loop1 stride 4 -> 8 -> 4 |
| nddma_scenarios_state_axis2 | 8 | 358 | 357 | loop2 stride 16 -> 32 -> 16 |
| nddma_scenarios_state_axis3 | 9 | 406 | 405 | loop3 stride 64 -> 128 -> 64 |
| nddma_scenarios_state_axis4 | 10 | 598 | 597 | loop4 stride 256 -> 512 -> 256 |
| nddma_scenarios_singleton_max_stride | 11 | 2 | 1 | Maximum legal source/destination strides on singleton axes |
| nddma_scenarios_zero_stride_5d | 12 | 2 | 1 | Zero source-stride broadcast on four active axes |

Profiles 6-10 perform three consecutive transfers in the same block and launch.
All five loop sizes are 2. Initial source strides are `{1,4,16,64,256}`, and destination
strides are `{1,2,4,8,16}`. The first and last transfers have a maximum accessed offset
of 341; only the middle transfer increases the selected source stride.
Valid cases require `device_operations=3 errors=0 warnings=0`. OOB cases require
`device_operations=3 errors=1 warnings=0`, with only the last byte of the middle transfer
outside the allocation. This detects both missed errors caused by ignoring the new stride
and false positives caused by retaining the increased stride after restoration.

Profile 11 uses loop sizes `{2,1,1,1,1}`. The last four axes have source strides of
`2^40-1` and destination strides of `2^20-1`. Singleton axes do not increase the access
extent, so only two bytes are read. This checks legal field maxima combined with singleton
axes; it does not claim coverage of transfers spanning 1 TB.

Profile 12 uses loop sizes of 2 on all axes, source strides `{1,0,0,0,0}`, and destination
strides `{1,2,4,8,16}`, broadcasting two source bytes into 32 destination bytes.

Verdicts use customer-visible GM out-of-bounds diagnostics, summaries, CLI results, and
complete sessions. They do not inspect internal raw, register, or CBData logs. Conversion
failures retain the existing skip-and-internal-log behavior. These cases do not exhaust
all loop sizes, padding configurations, or data types, and do not inject illegal raw
arithmetic overflows.

## Running

Load the CANN environment, ensure `npu-check` is available, and run from this directory:

```bash
export NPU_CHECK_E2E_REUSE_EXISTING_BUILD=1
bash check_memory_access_end_to_end.sh nddma_scenarios_state_axis0_valid
bash check_memory_access_end_to_end.sh nddma_scenarios_state_axis0_oob
for name in $(bash check_memory_access_end_to_end.sh --list | grep -E 'nddma_scenarios_(state_axis|singleton_max_stride|zero_stride_5d)'); do
    bash check_memory_access_end_to_end.sh "$name" || break
done
```

Logs are stored in `build/memory_access_logs/<case-name>.log`.

Compile and check the axis0 OOB case individually:

```bash
bisheng -xasc nddma_scenarios_memory_case.asc --npu-arch=dav-3510 -g \
    -DNDDMA_SCENARIO_PROFILE=6 -DNDDMA_SCENARIO_SOURCE_BYTES=342 \
    -o /tmp/nddma_state_axis0_oob -lacl_rt
npu-check --tool memcheck -- /tmp/nddma_state_axis0_oob
```

## Device Results

On 2026-09-07, all 14 new cases passed on dav-3510 in the `rj` Docker container with
CANN 9.2.0, using the installed `npu-check` without changes to the sanitizer implementation.
All seven valid cases reported `errors=0 warnings=0`, and all seven OOB cases reported
`errors=1 warnings=0`. Every case had `DEVICE_SYNC_RESULT=0`, a complete session, and
`child_exit=0`. Reports for all five axis-update OOB cases identified the second `DataCopy`;
the first and last transfers produced no out-of-bounds reports.
The existing `nddma_scenarios_broadcast_b8_valid` and `nddma_scenarios_multi_block_b8_oob`
cases also passed regression testing (2/2).

This validation does not constitute a rerun of all 244 cases or exhaustive coverage of
all legal arithmetic boundaries.
